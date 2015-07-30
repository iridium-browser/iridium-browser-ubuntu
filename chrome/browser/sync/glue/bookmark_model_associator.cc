// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_model_associator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/bookmark_undo_utils.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/delete_journal.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/syncapi_internal.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/util/cryptographer.h"
#include "sync/util/data_type_histogram.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace browser_sync {

// The sync protocol identifies top-level entities by means of well-known tags,
// which should not be confused with titles.  Each tag corresponds to a
// singleton instance of a particular top-level node in a user's share; the
// tags are consistent across users. The tags allow us to locate the specific
// folders whose contents we care about synchronizing, without having to do a
// lookup by name or path.  The tags should not be made user-visible.
// For example, the tag "bookmark_bar" represents the permanent node for
// bookmarks bar in Chrome. The tag "other_bookmarks" represents the permanent
// folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing,
// the sync server) to create these tagged nodes when initializing sync
// for the first time for a user.  Thus, once the backend finishes
// initializing, the ProfileSyncService can rely on the presence of tagged
// nodes.
//
// TODO(ncarter): Pull these tags from an external protocol specification
// rather than hardcoding them here.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Maximum number of bytes to allow in a title (must match sync's internal
// limits; see write_node.cc).
const int kTitleLimitBytes = 255;

// Provides the following abstraction: given a parent bookmark node, find best
// matching child node for many sync nodes.
class BookmarkNodeFinder {
 public:
  // Creates an instance with the given parent bookmark node.
  explicit BookmarkNodeFinder(const BookmarkNode* parent_node);

  // Finds the bookmark node that matches the given url, title and folder
  // attribute. Returns the matching node if one exists; NULL otherwise.
  // If there are multiple matches then a node with ID matching |preferred_id|
  // is returned; otherwise the first matching node is returned.
  // If a matching node is found, it's removed for further matches.
  const BookmarkNode* FindBookmarkNode(const GURL& url,
                                       const std::string& title,
                                       bool is_folder,
                                       int64 preferred_id);

  // Returns true if |bookmark_node| matches the specified |url|,
  // |title|, and |is_folder| flags.
  static bool NodeMatches(const BookmarkNode* bookmark_node,
                          const GURL& url,
                          const std::string& title,
                          bool is_folder);

 private:
  // Maps bookmark node titles to instances, duplicates allowed.
  // Titles are converted to the sync internal format before
  // being used as keys for the map.
  typedef base::hash_multimap<std::string,
                              const BookmarkNode*> BookmarkNodeMap;
  typedef std::pair<BookmarkNodeMap::iterator,
                    BookmarkNodeMap::iterator> BookmarkNodeRange;

  // Converts and truncates bookmark titles in the form sync does internally
  // to avoid mismatches due to sync munging titles.
  static void ConvertTitleToSyncInternalFormat(const std::string& input,
                                               std::string* output);

  const BookmarkNode* parent_node_;
  BookmarkNodeMap child_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeFinder);
};

class ScopedAssociationUpdater {
 public:
  explicit ScopedAssociationUpdater(BookmarkModel* model) {
    model_ = model;
    model->BeginExtensiveChanges();
  }

  ~ScopedAssociationUpdater() {
    model_->EndExtensiveChanges();
  }

 private:
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAssociationUpdater);
};

BookmarkNodeFinder::BookmarkNodeFinder(const BookmarkNode* parent_node)
    : parent_node_(parent_node) {
  for (int i = 0; i < parent_node_->child_count(); ++i) {
    const BookmarkNode* child_node = parent_node_->GetChild(i);

    std::string title = base::UTF16ToUTF8(child_node->GetTitle());
    ConvertTitleToSyncInternalFormat(title, &title);

    child_nodes_.insert(std::make_pair(title, child_node));
  }
}

const BookmarkNode* BookmarkNodeFinder::FindBookmarkNode(
    const GURL& url,
    const std::string& title,
    bool is_folder,
    int64 preferred_id) {
  const BookmarkNode* match = nullptr;

  // First lookup a range of bookmarks with the same title.
  std::string adjusted_title;
  ConvertTitleToSyncInternalFormat(title, &adjusted_title);
  BookmarkNodeRange range = child_nodes_.equal_range(adjusted_title);
  BookmarkNodeMap::iterator match_iter = range.second;
  for (BookmarkNodeMap::iterator iter = range.first;
       iter != range.second;
       ++iter) {
    // Then within the range match the node by the folder bit
    // and the url.
    const BookmarkNode* node = iter->second;
    if (is_folder == node->is_folder() && url == node->url()) {
      if (node->id() == preferred_id || preferred_id == 0) {
        // Preferred match - use this node.
        match = node;
        match_iter = iter;
        break;
      } else if (match == nullptr) {
        // First match - continue iterating.
        match = node;
        match_iter = iter;
      }
    }
  }

  if (match_iter != range.second) {
    // Remove the matched node so we don't match with it again.
    child_nodes_.erase(match_iter);
  }

  return match;
}

/* static */
bool BookmarkNodeFinder::NodeMatches(const BookmarkNode* bookmark_node,
                                     const GURL& url,
                                     const std::string& title,
                                     bool is_folder) {
  if (url != bookmark_node->url() || is_folder != bookmark_node->is_folder()) {
    return false;
  }

  // The title passed to this method comes from a sync directory entry.
  // The following two lines are needed to make the native bookmark title
  // comparable. The same conversion is used in BookmarkNodeFinder constructor.
  std::string bookmark_title = base::UTF16ToUTF8(bookmark_node->GetTitle());
  ConvertTitleToSyncInternalFormat(bookmark_title, &bookmark_title);
  return title == bookmark_title;
}

/* static */
void BookmarkNodeFinder::ConvertTitleToSyncInternalFormat(
    const std::string& input, std::string* output) {
  syncer::SyncAPINameToServerName(input, output);
  base::TruncateUTF8ToByteSize(*output, kTitleLimitBytes, output);
}

BookmarkModelAssociator::Context::Context(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result)
    : local_merge_result_(local_merge_result),
      syncer_merge_result_(syncer_merge_result),
      duplicate_count_(0) {
}

BookmarkModelAssociator::Context::~Context() {
}

void BookmarkModelAssociator::Context::PushNode(int64 sync_id) {
  dfs_stack_.push(sync_id);
}

bool BookmarkModelAssociator::Context::PopNode(int64* sync_id) {
  if (dfs_stack_.empty()) {
    *sync_id = 0;
    return false;
  }
  *sync_id = dfs_stack_.top();
  dfs_stack_.pop();
  return true;
}

void BookmarkModelAssociator::Context::SetPreAssociationVersions(
    int64 native_version,
    int64 sync_version) {
  local_merge_result_->set_pre_association_version(native_version);
  syncer_merge_result_->set_pre_association_version(sync_version);
}

void BookmarkModelAssociator::Context::SetNumItemsBeforeAssociation(
    int local_num,
    int sync_num) {
  local_merge_result_->set_num_items_before_association(local_num);
  syncer_merge_result_->set_num_items_before_association(sync_num);
}

void BookmarkModelAssociator::Context::SetNumItemsAfterAssociation(
    int local_num,
    int sync_num) {
  local_merge_result_->set_num_items_after_association(local_num);
  syncer_merge_result_->set_num_items_after_association(sync_num);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsDeleted() {
  local_merge_result_->set_num_items_deleted(
      local_merge_result_->num_items_deleted() + 1);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsAdded() {
  local_merge_result_->set_num_items_added(
      local_merge_result_->num_items_added() + 1);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsModified() {
  local_merge_result_->set_num_items_modified(
      local_merge_result_->num_items_modified() + 1);
}

void BookmarkModelAssociator::Context::IncrementSyncItemsAdded() {
  syncer_merge_result_->set_num_items_added(
      syncer_merge_result_->num_items_added() + 1);
}

void BookmarkModelAssociator::Context::UpdateDuplicateCount(
    const base::string16& title,
    const GURL& url) {
  // base::Hash is defined for 8-byte strings only so have to
  // cast the title data to char* and double the length in order to
  // compute its hash.
  size_t bookmark_hash = base::Hash(reinterpret_cast<const char*>(title.data()),
                                    title.length() * 2) ^
                         base::Hash(url.spec());

  if (!hashes_.insert(bookmark_hash).second) {
    // This hash code already exists in the set.
    ++duplicate_count_;
  }
}

BookmarkModelAssociator::BookmarkModelAssociator(
    BookmarkModel* bookmark_model,
    Profile* profile,
    syncer::UserShare* user_share,
    sync_driver::DataTypeErrorHandler* unrecoverable_error_handler,
    bool expect_mobile_bookmarks_folder)
    : bookmark_model_(bookmark_model),
      profile_(profile),
      user_share_(user_share),
      unrecoverable_error_handler_(unrecoverable_error_handler),
      expect_mobile_bookmarks_folder_(expect_mobile_bookmarks_folder),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bookmark_model_);
  DCHECK(user_share_);
  DCHECK(unrecoverable_error_handler_);
}

BookmarkModelAssociator::~BookmarkModelAssociator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BookmarkModelAssociator::UpdatePermanentNodeVisibility() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(bookmark_model_->loaded());

  BookmarkNode::Type bookmark_node_types[] = {
    BookmarkNode::BOOKMARK_BAR,
    BookmarkNode::OTHER_NODE,
    BookmarkNode::MOBILE,
  };
  for (size_t i = 0; i < arraysize(bookmark_node_types); ++i) {
    int64 id = bookmark_model_->PermanentNode(bookmark_node_types[i])->id();
    bookmark_model_->SetPermanentNodeVisible(
      bookmark_node_types[i],
      id_map_.find(id) != id_map_.end());
  }

  // Note: the root node may have additional extra nodes. Currently their
  // visibility is not affected by sync.
}

syncer::SyncError BookmarkModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
  return syncer::SyncError();
}

int64 BookmarkModelAssociator::GetSyncIdFromChromeId(const int64& node_id) {
  BookmarkIdToSyncIdMap::const_iterator iter = id_map_.find(node_id);
  return iter == id_map_.end() ? syncer::kInvalidId : iter->second;
}

const BookmarkNode* BookmarkModelAssociator::GetChromeNodeFromSyncId(
    int64 sync_id) {
  SyncIdToBookmarkNodeMap::const_iterator iter = id_map_inverse_.find(sync_id);
  return iter == id_map_inverse_.end() ? NULL : iter->second;
}

bool BookmarkModelAssociator::InitSyncNodeFromChromeId(
    const int64& node_id,
    syncer::BaseNode* sync_node) {
  DCHECK(sync_node);
  int64 sync_id = GetSyncIdFromChromeId(node_id);
  if (sync_id == syncer::kInvalidId)
    return false;
  if (sync_node->InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK)
    return false;
  DCHECK(sync_node->GetId() == sync_id);
  return true;
}

void BookmarkModelAssociator::AddAssociation(const BookmarkNode* node,
                                             int64 sync_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int64 node_id = node->id();
  DCHECK_NE(sync_id, syncer::kInvalidId);
  DCHECK(id_map_.find(node_id) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[node_id] = sync_id;
  id_map_inverse_[sync_id] = node;
}

void BookmarkModelAssociator::Associate(const BookmarkNode* node,
                                        const syncer::BaseNode& sync_node) {
  AddAssociation(node, sync_node.GetId());

  // TODO(stanisc): crbug.com/456876: consider not doing this on every single
  // association.
  UpdatePermanentNodeVisibility();

  // The same check exists in PersistAssociations. However it is better to
  // do the check earlier to avoid the cost of decrypting nodes again
  // in PersistAssociations.
  if (node->id() != sync_node.GetExternalId()) {
    dirty_associations_sync_ids_.insert(sync_node.GetId());
    PostPersistAssociationsTask();
  }
}

void BookmarkModelAssociator::Disassociate(int64 sync_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SyncIdToBookmarkNodeMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second->id());
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool BookmarkModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  bool has_mobile_folder = true;

  syncer::ReadTransaction trans(FROM_HERE, user_share_);

  syncer::ReadNode bookmark_bar_node(&trans);
  if (bookmark_bar_node.InitByTagLookupForBookmarks(kBookmarkBarTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  syncer::ReadNode other_bookmarks_node(&trans);
  if (other_bookmarks_node.InitByTagLookupForBookmarks(kOtherBookmarksTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  syncer::ReadNode mobile_bookmarks_node(&trans);
  if (mobile_bookmarks_node.InitByTagLookupForBookmarks(kMobileBookmarksTag) !=
      syncer::BaseNode::INIT_OK) {
    has_mobile_folder = false;
  }

  // Sync model has user created nodes if any of the permanent nodes has
  // children.
  *has_nodes = bookmark_bar_node.HasChildren() ||
      other_bookmarks_node.HasChildren() ||
      (has_mobile_folder && mobile_bookmarks_node.HasChildren());
  return true;
}

bool BookmarkModelAssociator::AssociateTaggedPermanentNode(
    syncer::BaseTransaction* trans,
    const BookmarkNode* permanent_node,
    const std::string& tag) {
  // Do nothing if |permanent_node| is already initialized and associated.
  int64 sync_id = GetSyncIdFromChromeId(permanent_node->id());
  if (sync_id != syncer::kInvalidId)
    return true;

  syncer::ReadNode sync_node(trans);
  if (sync_node.InitByTagLookupForBookmarks(tag) != syncer::BaseNode::INIT_OK)
    return false;

  Associate(permanent_node, sync_node);
  return true;
}

syncer::SyncError BookmarkModelAssociator::AssociateModels(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result) {
  // Since any changes to the bookmark model made here are not user initiated,
  // these change should not be undoable and so suspend the undo tracking.
  ScopedSuspendBookmarkUndo suspend_undo(
      BookmarkUndoServiceFactory::GetForProfileIfExists(profile_));

  Context context(local_merge_result, syncer_merge_result);

  syncer::SyncError error = CheckModelSyncState(&context);
  if (error.IsSet())
    return error;

  scoped_ptr<ScopedAssociationUpdater> association_updater(
      new ScopedAssociationUpdater(bookmark_model_));
  DisassociateModels();

  return BuildAssociations(&context);
}

syncer::SyncError BookmarkModelAssociator::AssociatePermanentFolders(
    syncer::BaseTransaction* trans,
    Context* context) {
  // To prime our association, we associate the top-level nodes, Bookmark Bar
  // and Other Bookmarks.
  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->bookmark_bar_node(),
                                    kBookmarkBarTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Bookmark bar node not found", model_type());
  }

  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->other_node(),
                                    kOtherBookmarksTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Other bookmarks node not found", model_type());
  }

  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->mobile_node(),
                                    kMobileBookmarksTag) &&
      expect_mobile_bookmarks_folder_) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Mobile bookmarks node not found", model_type());
  }

  // Note: the root node may have additional extra nodes. Currently none of
  // them are meant to sync.
  int64 bookmark_bar_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->bookmark_bar_node()->id());
  DCHECK_NE(bookmark_bar_sync_id, syncer::kInvalidId);
  int64 other_bookmarks_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->other_node()->id());
  DCHECK_NE(other_bookmarks_sync_id, syncer::kInvalidId);
  int64 mobile_bookmarks_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->mobile_node()->id());
  if (expect_mobile_bookmarks_folder_) {
    DCHECK_NE(syncer::kInvalidId, mobile_bookmarks_sync_id);
  }

  // WARNING: The order in which we push these should match their order in the
  // bookmark model (see BookmarkModel::DoneLoading(..)).
  context->PushNode(bookmark_bar_sync_id);
  context->PushNode(other_bookmarks_sync_id);
  if (mobile_bookmarks_sync_id != syncer::kInvalidId)
    context->PushNode(mobile_bookmarks_sync_id);

  return syncer::SyncError();
}

void BookmarkModelAssociator::SetNumItemsBeforeAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::BOOKMARKS) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsBeforeAssociation(
      GetTotalBookmarkCountAndRecordDuplicates(bookmark_model_->root_node(),
                                               context),
      syncer_num);
}

int BookmarkModelAssociator::GetTotalBookmarkCountAndRecordDuplicates(
    const bookmarks::BookmarkNode* node,
    Context* context) const {
  int count = 1;  // Start with one to include the node itself.

  if (!node->is_root()) {
    context->UpdateDuplicateCount(node->GetTitle(), node->url());
  }

  for (int i = 0; i < node->child_count(); ++i) {
    count +=
        GetTotalBookmarkCountAndRecordDuplicates(node->GetChild(i), context);
  }

  return count;
}

void BookmarkModelAssociator::SetNumItemsAfterAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::BOOKMARKS) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsAfterAssociation(
      bookmark_model_->root_node()->GetTotalNodeCount(), syncer_num);
}

syncer::SyncError BookmarkModelAssociator::BuildAssociations(Context* context) {
  DCHECK(bookmark_model_->loaded());

  syncer::WriteTransaction trans(FROM_HERE, user_share_);

  syncer::SyncError error = AssociatePermanentFolders(&trans, context);
  if (error.IsSet())
    return error;

  SetNumItemsBeforeAssociation(&trans, context);

  int initial_duplicate_count = context->duplicate_count();

  // Remove obsolete bookmarks according to sync delete journal.
  // TODO(stanisc): crbug.com/456876: rewrite this to avoid a separate
  // traversal and instead perform deletes at the end of the loop below where
  // the unmatched bookmark nodes are created as sync nodes.
  ApplyDeletesFromSyncJournal(&trans, context);

  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each sync node for the current sync parent node, find the best
  //   matching bookmark node under the corresponding bookmark parent node.
  //   If no matching node is found, create a new bookmark node in the same
  //   position as the corresponding sync node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding sync node.
  // * When all children sync nodes are done, add the extra children bookmark
  //   nodes to the sync parent node.
  //
  // The best match algorithm uses folder title or bookmark title/url to
  // perform the primary match. If there are multiple match candidates it
  // selects the preferred one based on sync node external ID match to the
  // bookmark folder ID.
  int64 sync_parent_id;
  while (context->PopNode(&sync_parent_id)) {
    syncer::ReadNode sync_parent(&trans);
    if (sync_parent.InitByIdLookup(sync_parent_id) !=
        syncer::BaseNode::INIT_OK) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to lookup node.", model_type());
    }
    // Only folder nodes are pushed on to the stack.
    DCHECK(sync_parent.GetIsFolder());

    const BookmarkNode* parent_node = GetChromeNodeFromSyncId(sync_parent_id);
    if (!parent_node) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to find bookmark node for sync id.", model_type());
    }
    DCHECK(parent_node->is_folder());

    std::vector<int64> children;
    sync_parent.GetChildIds(&children);
    error = BuildAssociations(&trans, parent_node, children, context);
    if (error.IsSet())
      return error;
  }

  SetNumItemsAfterAssociation(&trans, context);

  UMA_HISTOGRAM_COUNTS("Sync.BookmarksDuplicationsAtAssociation",
                       context->duplicate_count());
  UMA_HISTOGRAM_COUNTS("Sync.BookmarksNewDuplicationsAtAssociation",
                       context->duplicate_count() - initial_duplicate_count);

  return syncer::SyncError();
}

syncer::SyncError BookmarkModelAssociator::BuildAssociations(
    syncer::WriteTransaction* trans,
    const BookmarkNode* parent_node,
    const std::vector<int64>& sync_ids,
    Context* context) {
  BookmarkNodeFinder node_finder(parent_node);

  int index = 0;
  for (std::vector<int64>::const_iterator it = sync_ids.begin();
       it != sync_ids.end(); ++it) {
    int64 sync_child_id = *it;
    syncer::ReadNode sync_child_node(trans);
    if (sync_child_node.InitByIdLookup(sync_child_id) !=
        syncer::BaseNode::INIT_OK) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to lookup node.", model_type());
    }

    GURL url(sync_child_node.GetBookmarkSpecifics().url());
    const BookmarkNode* child_node = node_finder.FindBookmarkNode(
        url, sync_child_node.GetTitle(), sync_child_node.GetIsFolder(),
        sync_child_node.GetExternalId());
    if (child_node) {
      // All bookmarks are currently modified at association time, even if
      // nothing has changed.
      // TODO(sync): Only modify the bookmark model if necessary.
      BookmarkChangeProcessor::UpdateBookmarkWithSyncData(
          sync_child_node, bookmark_model_, child_node, profile_);
      bookmark_model_->Move(child_node, parent_node, index);
      context->IncrementLocalItemsModified();
    } else {
      DCHECK_LE(index, parent_node->child_count());

      base::string16 title = base::UTF8ToUTF16(sync_child_node.GetTitle());
      child_node = BookmarkChangeProcessor::CreateBookmarkNode(
          title, url, &sync_child_node, parent_node, bookmark_model_, profile_,
          index);
      if (!child_node) {
        return unrecoverable_error_handler_->CreateAndUploadError(
            FROM_HERE, "Failed to create bookmark node with title " +
                           sync_child_node.GetTitle() + " and url " +
                           url.possibly_invalid_spec(),
            model_type());
      }
      context->UpdateDuplicateCount(title, url);
      context->IncrementLocalItemsAdded();
    }

    Associate(child_node, sync_child_node);

    if (sync_child_node.GetIsFolder())
      context->PushNode(sync_child_id);
    ++index;
  }

  // At this point all the children nodes of the parent sync node have
  // corresponding children in the parent bookmark node and they are all in
  // the right positions: from 0 to index - 1.
  // So the children starting from index in the parent bookmark node are the
  // ones that are not present in the parent sync node. So create them.
  for (int i = index; i < parent_node->child_count(); ++i) {
    int64 sync_child_id = BookmarkChangeProcessor::CreateSyncNode(
        parent_node, bookmark_model_, i, trans, this,
        unrecoverable_error_handler_);
    if (syncer::kInvalidId == sync_child_id) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to create sync node.", model_type());
    }
    context->IncrementSyncItemsAdded();
    if (parent_node->GetChild(i)->is_folder())
      context->PushNode(sync_child_id);
  }

  return syncer::SyncError();
}

struct FolderInfo {
  FolderInfo(const BookmarkNode* f, const BookmarkNode* p, int64 id)
      : folder(f), parent(p), sync_id(id) {}
  const BookmarkNode* folder;
  const BookmarkNode* parent;
  int64 sync_id;
};
typedef std::vector<FolderInfo> FolderInfoList;

void BookmarkModelAssociator::ApplyDeletesFromSyncJournal(
    syncer::BaseTransaction* trans,
    Context* context) {
  syncer::BookmarkDeleteJournalList bk_delete_journals;
  syncer::DeleteJournal::GetBookmarkDeleteJournals(trans, &bk_delete_journals);
  if (bk_delete_journals.empty())
    return;

  size_t num_journals_unmatched = bk_delete_journals.size();

  // Make a set of all external IDs in the delete journal,
  // ignore entries with unset external IDs.
  std::set<int64> journaled_external_ids;
  for (size_t i = 0; i < num_journals_unmatched; i++) {
    if (bk_delete_journals[i].external_id != 0)
      journaled_external_ids.insert(bk_delete_journals[i].external_id);
  }

  // Check bookmark model from top to bottom.
  std::stack<const BookmarkNode*> dfs_stack;
  dfs_stack.push(bookmark_model_->bookmark_bar_node());
  dfs_stack.push(bookmark_model_->other_node());
  if (expect_mobile_bookmarks_folder_)
    dfs_stack.push(bookmark_model_->mobile_node());
  // Note: the root node may have additional extra nodes. Currently none of
  // them are meant to sync.

  // Remember folders that match delete journals in first pass but don't delete
  // them in case there are bookmarks left under them. After non-folder
  // bookmarks are removed in first pass, recheck the folders in reverse order
  // to remove empty ones.
  FolderInfoList folders_matched;
  while (!dfs_stack.empty() && num_journals_unmatched > 0) {
    const BookmarkNode* parent = dfs_stack.top();
    dfs_stack.pop();
    DCHECK(parent->is_folder());

    // Enumerate folder children in reverse order to make it easier to remove
    // bookmarks matching entries in the delete journal.
    for (int child_index = parent->child_count() - 1;
         child_index >= 0 && num_journals_unmatched > 0; --child_index) {
      const BookmarkNode* child = parent->GetChild(child_index);
      if (child->is_folder())
        dfs_stack.push(child);

      if (journaled_external_ids.find(child->id()) ==
          journaled_external_ids.end()) {
        // Skip bookmark node which id is not in the set of external IDs.
        continue;
      }

      // Iterate through the journal entries from back to front. Remove matched
      // journal by moving an unmatched entry at the tail to the matched
      // position so that we can read unmatched entries off the head in next
      // loop.
      for (int journal_index = num_journals_unmatched - 1; journal_index >= 0;
           --journal_index) {
        const syncer::BookmarkDeleteJournal& delete_entry =
            bk_delete_journals[journal_index];
        if (child->id() == delete_entry.external_id &&
            BookmarkNodeFinder::NodeMatches(
                child, GURL(delete_entry.specifics.bookmark().url()),
                delete_entry.specifics.bookmark().title(),
                delete_entry.is_folder)) {
          if (child->is_folder()) {
            // Remember matched folder without removing and delete only empty
            // ones later.
            folders_matched.push_back(
                FolderInfo(child, parent, delete_entry.id));
          } else {
            bookmark_model_->Remove(child);
            context->IncrementLocalItemsDeleted();
          }
          // Move unmatched journal here and decrement counter.
          bk_delete_journals[journal_index] =
              bk_delete_journals[--num_journals_unmatched];
          break;
        }
      }
    }
  }

  // Ids of sync nodes not found in bookmark model, meaning the deletions are
  // persisted and correponding delete journals can be dropped.
  std::set<int64> journals_to_purge;

  // Remove empty folders from bottom to top.
  for (FolderInfoList::reverse_iterator it = folders_matched.rbegin();
      it != folders_matched.rend(); ++it) {
    if (it->folder->child_count() == 0) {
      bookmark_model_->Remove(it->folder);
      context->IncrementLocalItemsDeleted();
    } else {
      // Keep non-empty folder and remove its journal so that it won't match
      // again in the future.
      journals_to_purge.insert(it->sync_id);
    }
  }

  // Purge unmatched journals.
  for (size_t i = 0; i < num_journals_unmatched; ++i)
    journals_to_purge.insert(bk_delete_journals[i].id);
  syncer::DeleteJournal::PurgeDeleteJournals(trans, journals_to_purge);
}

void BookmarkModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (weak_factory_.HasWeakPtrs())
    return;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &BookmarkModelAssociator::PersistAssociations,
          weak_factory_.GetWeakPtr()));
}

void BookmarkModelAssociator::PersistAssociations() {
  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  int64 new_version = syncer::syncable::kInvalidTransactionVersion;
  std::vector<const BookmarkNode*> bnodes;
  {
    syncer::WriteTransaction trans(FROM_HERE, user_share_, &new_version);
    DirtyAssociationsSyncIds::iterator iter;
    for (iter = dirty_associations_sync_ids_.begin();
         iter != dirty_associations_sync_ids_.end();
         ++iter) {
      int64 sync_id = *iter;
      syncer::WriteNode sync_node(&trans);
      if (sync_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
        syncer::SyncError error(
            FROM_HERE,
            syncer::SyncError::DATATYPE_ERROR,
            "Could not lookup bookmark node for ID persistence.",
            syncer::BOOKMARKS);
        unrecoverable_error_handler_->OnSingleDataTypeUnrecoverableError(error);
        return;
      }
      const BookmarkNode* node = GetChromeNodeFromSyncId(sync_id);
      if (node && sync_node.GetExternalId() != node->id()) {
        sync_node.SetExternalId(node->id());
        bnodes.push_back(node);
      }
    }
    dirty_associations_sync_ids_.clear();
  }

  BookmarkChangeProcessor::UpdateTransactionVersion(new_version,
                                                    bookmark_model_,
                                                    bnodes);
}

bool BookmarkModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(syncer::BOOKMARKS) ||
      trans.GetCryptographer()->is_ready();
}

syncer::SyncError BookmarkModelAssociator::CheckModelSyncState(
    Context* context) const {
  int64 native_version =
      bookmark_model_->root_node()->sync_transaction_version();
  if (native_version != syncer::syncable::kInvalidTransactionVersion) {
    syncer::ReadTransaction trans(FROM_HERE, user_share_);
    int64 sync_version = trans.GetModelVersion(syncer::BOOKMARKS);
    context->SetPreAssociationVersions(native_version, sync_version);

    if (native_version != sync_version) {
      UMA_HISTOGRAM_ENUMERATION("Sync.LocalModelOutOfSync",
                                ModelTypeToHistogramInt(syncer::BOOKMARKS),
                                syncer::MODEL_TYPE_COUNT);

      // Clear version on bookmark model so that we only report error once.
      bookmark_model_->SetNodeSyncTransactionVersion(
          bookmark_model_->root_node(),
          syncer::syncable::kInvalidTransactionVersion);

      // If the native version is higher, there was a sync persistence failure,
      // and we need to delay association until after a GetUpdates.
      if (sync_version < native_version) {
        std::string message = base::StringPrintf(
            "Native version (%" PRId64 ") does not match sync version (%"
                PRId64 ")",
            native_version,
            sync_version);
        return syncer::SyncError(FROM_HERE,
                                 syncer::SyncError::PERSISTENCE_ERROR,
                                 message,
                                 syncer::BOOKMARKS);
      }
    }
  }
  return syncer::SyncError();
}

}  // namespace browser_sync
