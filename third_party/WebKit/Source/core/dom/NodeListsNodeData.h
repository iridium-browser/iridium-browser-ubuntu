/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NodeListsNodeData_h
#define NodeListsNodeData_h

#include "core/dom/ChildNodeList.h"
#include "core/dom/EmptyNodeList.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/TagCollection.h"
#include "core/html/CollectionType.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringHash.h"

namespace blink {

class NodeListsNodeData final : public GarbageCollected<NodeListsNodeData> {
  WTF_MAKE_NONCOPYABLE(NodeListsNodeData);

 public:
  ChildNodeList* childNodeList(ContainerNode& node) {
    DCHECK(!m_childNodeList || node == m_childNodeList->virtualOwnerNode());
    return toChildNodeList(m_childNodeList);
  }

  ChildNodeList* ensureChildNodeList(ContainerNode& node) {
    DCHECK(ThreadState::current()->isGCForbidden());
    if (m_childNodeList)
      return toChildNodeList(m_childNodeList);
    ChildNodeList* list = ChildNodeList::create(node);
    m_childNodeList = list;
    ScriptWrappableVisitor::writeBarrier(this, list);
    return list;
  }

  EmptyNodeList* ensureEmptyChildNodeList(Node& node) {
    DCHECK(ThreadState::current()->isGCForbidden());
    if (m_childNodeList)
      return toEmptyNodeList(m_childNodeList);
    EmptyNodeList* list = EmptyNodeList::create(node);
    m_childNodeList = list;
    ScriptWrappableVisitor::writeBarrier(this, list);
    return list;
  }

  struct NodeListAtomicCacheMapEntryHash {
    STATIC_ONLY(NodeListAtomicCacheMapEntryHash);
    static unsigned hash(const std::pair<unsigned char, StringImpl*>& entry) {
      return DefaultHash<StringImpl*>::Hash::hash(entry.second) + entry.first;
    }
    static bool equal(const std::pair<unsigned char, StringImpl*>& a,
                      const std::pair<unsigned char, StringImpl*>& b) {
      return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted =
        DefaultHash<StringImpl*>::Hash::safeToCompareToEmptyOrDeleted;
  };

  // Oilpan: keep a weak reference to the collection objects.
  // Object unregistration is handled by GC's weak processing.
  typedef HeapHashMap<std::pair<unsigned char, StringImpl*>,
                      WeakMember<LiveNodeListBase>,
                      NodeListAtomicCacheMapEntryHash>
      NodeListAtomicNameCacheMap;
  typedef HeapHashMap<QualifiedName, WeakMember<TagCollection>>
      TagCollectionCacheNS;

  template <typename T>
  T* addCache(ContainerNode& node,
              CollectionType collectionType,
              const AtomicString& name) {
    DCHECK(ThreadState::current()->isGCForbidden());
    NodeListAtomicNameCacheMap::AddResult result =
        m_atomicNameCaches.add(namedNodeListKey(collectionType, name), nullptr);
    if (!result.isNewEntry) {
      return static_cast<T*>(result.storedValue->value.get());
    }

    T* list = T::create(node, collectionType, name);
    result.storedValue->value = list;
    return list;
  }

  template <typename T>
  T* addCache(ContainerNode& node, CollectionType collectionType) {
    DCHECK(ThreadState::current()->isGCForbidden());
    NodeListAtomicNameCacheMap::AddResult result = m_atomicNameCaches.add(
        namedNodeListKey(collectionType, starAtom), nullptr);
    if (!result.isNewEntry) {
      return static_cast<T*>(result.storedValue->value.get());
    }

    T* list = T::create(node, collectionType);
    result.storedValue->value = list;
    return list;
  }

  template <typename T>
  T* cached(CollectionType collectionType) {
    return static_cast<T*>(
        m_atomicNameCaches.get(namedNodeListKey(collectionType, starAtom)));
  }

  TagCollection* addCache(ContainerNode& node,
                          const AtomicString& namespaceURI,
                          const AtomicString& localName) {
    DCHECK(ThreadState::current()->isGCForbidden());
    QualifiedName name(nullAtom, localName, namespaceURI);
    TagCollectionCacheNS::AddResult result =
        m_tagCollectionCacheNS.add(name, nullptr);
    if (!result.isNewEntry)
      return result.storedValue->value;

    TagCollection* list = TagCollection::create(node, namespaceURI, localName);
    result.storedValue->value = list;
    return list;
  }

  static NodeListsNodeData* create() { return new NodeListsNodeData; }

  void invalidateCaches(const QualifiedName* attrName = 0);

  bool isEmpty() const {
    return !m_childNodeList && m_atomicNameCaches.isEmpty() &&
           m_tagCollectionCacheNS.isEmpty();
  }

  void adoptTreeScope() { invalidateCaches(); }

  void adoptDocument(Document& oldDocument, Document& newDocument) {
    DCHECK_NE(oldDocument, newDocument);

    NodeListAtomicNameCacheMap::const_iterator atomicNameCacheEnd =
        m_atomicNameCaches.end();
    for (NodeListAtomicNameCacheMap::const_iterator it =
             m_atomicNameCaches.begin();
         it != atomicNameCacheEnd; ++it) {
      LiveNodeListBase* list = it->value;
      list->didMoveToDocument(oldDocument, newDocument);
    }

    TagCollectionCacheNS::const_iterator tagEnd = m_tagCollectionCacheNS.end();
    for (TagCollectionCacheNS::const_iterator it =
             m_tagCollectionCacheNS.begin();
         it != tagEnd; ++it) {
      LiveNodeListBase* list = it->value;
      DCHECK(!list->isRootedAtTreeScope());
      list->didMoveToDocument(oldDocument, newDocument);
    }
  }

  DECLARE_TRACE();

  DECLARE_TRACE_WRAPPERS();

 private:
  NodeListsNodeData() : m_childNodeList(nullptr) {}

  std::pair<unsigned char, StringImpl*> namedNodeListKey(
      CollectionType type,
      const AtomicString& name) {
    // Holding the raw StringImpl is safe because |name| is retained by the
    // NodeList and the NodeList is reponsible for removing itself from the
    // cache on deletion.
    return std::pair<unsigned char, StringImpl*>(type, name.impl());
  }

  // Can be a ChildNodeList or an EmptyNodeList.
  WeakMember<NodeList> m_childNodeList;
  NodeListAtomicNameCacheMap m_atomicNameCaches;
  TagCollectionCacheNS m_tagCollectionCacheNS;
};

template <typename Collection>
inline Collection* ContainerNode::ensureCachedCollection(CollectionType type) {
  ThreadState::MainThreadGCForbiddenScope gcForbidden;
  return ensureNodeLists().addCache<Collection>(*this, type);
}

template <typename Collection>
inline Collection* ContainerNode::ensureCachedCollection(
    CollectionType type,
    const AtomicString& name) {
  ThreadState::MainThreadGCForbiddenScope gcForbidden;
  return ensureNodeLists().addCache<Collection>(*this, type, name);
}

template <typename Collection>
inline Collection* ContainerNode::ensureCachedCollection(
    CollectionType type,
    const AtomicString& namespaceURI,
    const AtomicString& localName) {
  DCHECK_EQ(type, TagCollectionType);
  ThreadState::MainThreadGCForbiddenScope gcForbidden;
  return ensureNodeLists().addCache(*this, namespaceURI, localName);
}

template <typename Collection>
inline Collection* ContainerNode::cachedCollection(CollectionType type) {
  NodeListsNodeData* nodeLists = this->nodeLists();
  return nodeLists ? nodeLists->cached<Collection>(type) : 0;
}

}  // namespace blink

#endif  // NodeListsNodeData_h
