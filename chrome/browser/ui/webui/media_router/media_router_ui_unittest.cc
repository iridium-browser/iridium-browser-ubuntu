// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::SaveArg;
using testing::Return;

namespace media_router {

class PresentationRequestCallbacks {
public:
  explicit PresentationRequestCallbacks(
      const content::PresentationError& expected_error)
      : expected_error_(expected_error) {}

  void Success(const content::PresentationSessionInfo&, const MediaRoute::Id&) {
  }

  void Error(const content::PresentationError& error) {
    EXPECT_EQ(expected_error_.error_type, error.error_type);
    EXPECT_EQ(expected_error_.message, error.message);
  }

private:
  content::PresentationError expected_error_;
};

class MockRoutesUpdatedCallback {
 public:
  MOCK_METHOD2(OnRoutesUpdated,
               void(const std::vector<MediaRoute>& routes,
                    const std::vector<MediaRoute::Id>& joinable_route_ids));
};

class MediaRouterUITest : public ::testing::Test {
 public:
  ~MediaRouterUITest() override {
    EXPECT_CALL(mock_router_, UnregisterMediaSinksObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_))
        .Times(AnyNumber());
  }

  void CreateMediaRouterUI(Profile* profile) {
    initiator_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    SessionTabHelper::CreateForWebContents(initiator_.get());
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile)));
    web_ui_.set_web_contents(web_contents_.get());
    media_router_ui_.reset(new MediaRouterUI(&web_ui_));
    message_handler_.reset(
        new MediaRouterWebUIMessageHandler(media_router_ui_.get()));
    EXPECT_CALL(mock_router_, RegisterMediaSinksObserver(_))
        .WillRepeatedly(Invoke([this](MediaSinksObserver* observer) {
          this->media_sinks_observers_.push_back(observer);
          return true;
        }));
    EXPECT_CALL(mock_router_, RegisterMediaRoutesObserver(_))
        .Times(AnyNumber());
    media_router_ui_->InitForTest(&mock_router_, initiator_.get(),
                                  message_handler_.get(),
                                  std::move(create_session_request_));
    message_handler_->SetWebUIForTest(&web_ui_);
  }

 protected:
  MockMediaRouter mock_router_;
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> initiator_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CreatePresentationConnectionRequest> create_session_request_;
  std::unique_ptr<MediaRouterUI> media_router_ui_;
  std::unique_ptr<MediaRouterWebUIMessageHandler> message_handler_;
  std::vector<MediaSinksObserver*> media_sinks_observers_;
};

TEST_F(MediaRouterUITest, RouteCreationTimeoutForTab) {
  CreateMediaRouterUI(&profile_);
  std::vector<MediaRouteResponseCallback> callbacks;
  EXPECT_CALL(
      mock_router_,
      CreateRoute(_, _, _, _, _, base::TimeDelta::FromSeconds(60), false))
      .WillOnce(SaveArg<4>(&callbacks));
  media_router_ui_->CreateRoute("sinkId", MediaCastMode::TAB_MIRROR);

  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  EXPECT_CALL(mock_router_, AddIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  for (const auto& callback : callbacks)
    callback.Run(*result);
}

TEST_F(MediaRouterUITest, RouteCreationTimeoutForDesktop) {
  CreateMediaRouterUI(&profile_);
  std::vector<MediaRouteResponseCallback> callbacks;
  EXPECT_CALL(
      mock_router_,
      CreateRoute(_, _, _, _, _, base::TimeDelta::FromSeconds(120), false))
      .WillOnce(SaveArg<4>(&callbacks));
  media_router_ui_->CreateRoute("sinkId", MediaCastMode::DESKTOP_MIRROR);

  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP);
  EXPECT_CALL(mock_router_, AddIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  for (const auto& callback : callbacks)
    callback.Run(*result);
}

TEST_F(MediaRouterUITest, RouteCreationTimeoutForPresentation) {
  CreateMediaRouterUI(&profile_);
  PresentationRequest presentation_request(RenderFrameHostId(0, 0),
                                           "https://presentationurl.fakeurl",
                                           GURL("https://frameurl.fakeurl"));
  media_router_ui_->OnDefaultPresentationChanged(presentation_request);
  std::vector<MediaRouteResponseCallback> callbacks;
  EXPECT_CALL(
      mock_router_,
      CreateRoute(_, _, _, _, _, base::TimeDelta::FromSeconds(20), false))
      .WillOnce(SaveArg<4>(&callbacks));
  media_router_ui_->CreateRoute("sinkId", MediaCastMode::DEFAULT);

  std::string expected_title =
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                base::UTF8ToUTF16("frameurl.fakeurl"));
  EXPECT_CALL(mock_router_, AddIssue(IssueTitleEquals(expected_title)));
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromError("Timed out", RouteRequestResult::TIMED_OUT);
  for (const auto& callback : callbacks)
    callback.Run(*result);
}

TEST_F(MediaRouterUITest, RouteCreationParametersCantBeCreated) {
  CreateMediaRouterUI(&profile_);
  MediaSinkSearchResponseCallback sink_callback;
  EXPECT_CALL(mock_router_, SearchSinks(_, _, _, _, _))
      .WillOnce(SaveArg<4>(&sink_callback));

  // Use DEFAULT mode without setting a PresentationRequest.
  media_router_ui_->SearchSinksAndCreateRoute("sinkId", "search input",
                                              "domain", MediaCastMode::DEFAULT);
  std::string expected_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  EXPECT_CALL(mock_router_, AddIssue(IssueTitleEquals(expected_title)));
  sink_callback.Run("foundSinkId");
}

TEST_F(MediaRouterUITest, RouteRequestFromIncognito) {
  CreateMediaRouterUI(profile_.GetOffTheRecordProfile());

  PresentationRequest presentation_request(
      RenderFrameHostId(0, 0), "https://fooUrl", GURL("https://frameUrl"));
  media_router_ui_->OnDefaultPresentationChanged(presentation_request);

  EXPECT_CALL(
      mock_router_,
      CreateRoute(_, _, _, _, _, base::TimeDelta::FromSeconds(20), true));
  media_router_ui_->CreateRoute("sinkId", MediaCastMode::DEFAULT);
}

TEST_F(MediaRouterUITest, SortedSinks) {
  CreateMediaRouterUI(&profile_);
  std::vector<MediaSinkWithCastModes> unsorted_sinks;
  std::string sink_id1("sink3");
  std::string sink_name1("B sink");
  MediaSinkWithCastModes sink1(
      MediaSink(sink_id1, sink_name1, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink1);

  std::string sink_id2("sink1");
  std::string sink_name2("A sink");
  MediaSinkWithCastModes sink2(
      MediaSink(sink_id2, sink_name2, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink2);

  std::string sink_id3("sink2");
  std::string sink_name3("B sink");
  MediaSinkWithCastModes sink3(
      MediaSink(sink_id3, sink_name3, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink3);

  // Sorted order is 2, 3, 1.
  media_router_ui_->OnResultsUpdated(unsorted_sinks);
  const auto& sorted_sinks = media_router_ui_->sinks_;
  EXPECT_EQ(sink_name2, sorted_sinks[0].sink.name());
  EXPECT_EQ(sink_id3, sorted_sinks[1].sink.id());
  EXPECT_EQ(sink_id1, sorted_sinks[2].sink.id());
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverFiltersNonDisplayRoutes) {
  EXPECT_CALL(mock_router_, RegisterMediaRoutesObserver(_)).Times(1);
  MediaSource media_source("mediaSource");
  MockRoutesUpdatedCallback mock_callback;
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, media_source.id(),
          base::Bind(&MockRoutesUpdatedCallback::OnRoutesUpdated,
                     base::Unretained(&mock_callback))));

  MediaRoute display_route_1("routeId1", media_source, "sinkId1", "desc 1",
                             true, "", true);
  MediaRoute non_display_route_1("routeId2", media_source, "sinkId2", "desc 2",
                                 true, "", false);
  MediaRoute display_route_2("routeId3", media_source, "sinkId2", "desc 2",
                             true, "", true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  std::vector<MediaRoute> filtered_routes;
  EXPECT_CALL(mock_callback, OnRoutesUpdated(_, _))
      .WillOnce(SaveArg<0>(&filtered_routes));
  observer->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());

  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest,
    UIMediaRoutesObserverFiltersNonDisplayJoinableRoutes) {
  EXPECT_CALL(mock_router_, RegisterMediaRoutesObserver(_)).Times(1);
  MediaSource media_source("mediaSource");
  MockRoutesUpdatedCallback mock_callback;
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, media_source.id(),
          base::Bind(&MockRoutesUpdatedCallback::OnRoutesUpdated,
                     base::Unretained(&mock_callback))));

  MediaRoute display_route_1("routeId1", media_source, "sinkId1", "desc 1",
                             true, "", true);
  MediaRoute non_display_route_1("routeId2", media_source, "sinkId2", "desc 2",
                                 true, "", false);
  MediaRoute display_route_2("routeId3", media_source, "sinkId2", "desc 2",
                             true, "", true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  std::vector<MediaRoute::Id> joinable_route_ids;
  joinable_route_ids.push_back("routeId1");
  joinable_route_ids.push_back("routeId2");
  joinable_route_ids.push_back("routeId3");

  std::vector<MediaRoute::Id> filtered_joinable_route_ids;
  // Save the filtered joinable routes.
  EXPECT_CALL(mock_callback, OnRoutesUpdated(_, _))
      .WillOnce(SaveArg<1>(&filtered_joinable_route_ids));
  observer->OnRoutesUpdated(routes, joinable_route_ids);

  ASSERT_EQ(2u, filtered_joinable_route_ids.size());
  EXPECT_EQ(display_route_1.media_route_id(), filtered_joinable_route_ids[0]);
  EXPECT_EQ(display_route_2.media_route_id(), filtered_joinable_route_ids[1]);

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverAssignsCurrentCastModes) {
  CreateMediaRouterUI(&profile_);
  SessionID::id_type tab_id = SessionTabHelper::IdForTab(initiator_.get());
  MediaSource media_source_1(MediaSourceForTab(tab_id));
  MediaSource media_source_2("mediaSource");
  MediaSource media_source_3(MediaSourceForDesktop());
  MockRoutesUpdatedCallback mock_callback;
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, MediaSource::Id(),
          base::Bind(&MediaRouterUI::OnRoutesUpdated,
                     base::Unretained(media_router_ui_.get()))));

  MediaRoute display_route_1("routeId1", media_source_1, "sinkId1", "desc 1",
                             true, "", true);
  MediaRoute non_display_route_1("routeId2", media_source_2, "sinkId2",
                                 "desc 2", true, "", false);
  MediaRoute display_route_2("routeId3", media_source_3, "sinkId2", "desc 2",
                             true, "", true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  observer->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());

  const auto& filtered_routes = media_router_ui_->routes();
  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  const auto& current_cast_modes = media_router_ui_->current_cast_modes();
  ASSERT_EQ(2u, current_cast_modes.size());
  auto cast_mode_entry =
      current_cast_modes.find(display_route_1.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::TAB_MIRROR, cast_mode_entry->second);
  cast_mode_entry =
      current_cast_modes.find(non_display_route_1.media_route_id());
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry = current_cast_modes.find(display_route_2.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR, cast_mode_entry->second);

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverSkipsUnavailableCastModes) {
  CreateMediaRouterUI(&profile_);
  MediaSource media_source_1("mediaSource1");
  MediaSource media_source_2("mediaSource2");
  MediaSource media_source_3(MediaSourceForDesktop());
  MockRoutesUpdatedCallback mock_callback;
  std::unique_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router_, MediaSource::Id(),
          base::Bind(&MediaRouterUI::OnRoutesUpdated,
                     base::Unretained(media_router_ui_.get()))));

  MediaRoute display_route_1("routeId1", media_source_1, "sinkId1", "desc 1",
                             true, "", true);
  MediaRoute non_display_route_1("routeId2", media_source_2, "sinkId2",
                                 "desc 2", true, "", false);
  MediaRoute display_route_2("routeId3", media_source_3, "sinkId2", "desc 2",
                             true, "", true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  observer->OnRoutesUpdated(routes, std::vector<MediaRoute::Id>());

  const auto& filtered_routes = media_router_ui_->routes();
  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  const auto& current_cast_modes = media_router_ui_->current_cast_modes();
  ASSERT_EQ(1u, current_cast_modes.size());
  auto cast_mode_entry =
      current_cast_modes.find(display_route_1.media_route_id());
  // No observer for source "mediaSource1" means no cast mode for this route.
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry =
      current_cast_modes.find(non_display_route_1.media_route_id());
  EXPECT_EQ(end(current_cast_modes), cast_mode_entry);
  cast_mode_entry = current_cast_modes.find(display_route_2.media_route_id());
  EXPECT_NE(end(current_cast_modes), cast_mode_entry);
  EXPECT_EQ(MediaCastMode::DESKTOP_MIRROR, cast_mode_entry->second);

  EXPECT_CALL(mock_router_, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, GetExtensionNameExtensionPresent) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      base::WrapUnique(new extensions::ExtensionRegistry(nullptr));
  scoped_refptr<extensions::Extension> app =
      extensions::test_util::BuildApp(extensions::ExtensionBuilder())
          .MergeManifest(extensions::DictionaryBuilder()
                             .Set("name", "test app name")
                             .Build())
          .SetID(id)
          .Build();

  ASSERT_TRUE(registry->AddEnabled(app));
  EXPECT_EQ("test app name",
            MediaRouterUI::GetExtensionName(url, registry.get()));
}

TEST_F(MediaRouterUITest, GetExtensionNameEmptyWhenNotInstalled) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      base::WrapUnique(new extensions::ExtensionRegistry(nullptr));

  EXPECT_EQ("", MediaRouterUI::GetExtensionName(url, registry.get()));
}

TEST_F(MediaRouterUITest, GetExtensionNameEmptyWhenNotExtensionURL) {
  GURL url = GURL("https://www.google.com");
  std::unique_ptr<extensions::ExtensionRegistry> registry =
      base::WrapUnique(new extensions::ExtensionRegistry(nullptr));

  EXPECT_EQ("", MediaRouterUI::GetExtensionName(url, registry.get()));
}

TEST_F(MediaRouterUITest, NotFoundErrorOnCloseWithNoSinks) {
  content::PresentationError expected_error(
      content::PresentationErrorType::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
      "No screens found.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  create_session_request_.reset(new CreatePresentationConnectionRequest(
      RenderFrameHostId(0, 0), std::string("http://google.com/presentation"),
      GURL("http://google.com"),
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks))));
  CreateMediaRouterUI(&profile_);
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}

TEST_F(MediaRouterUITest, NotFoundErrorOnCloseWithNoCompatibleSinks) {
  content::PresentationError expected_error(
      content::PresentationErrorType::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
      "No screens found.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  std::string presentation_url("http://google.com/presentation");
  create_session_request_.reset(new CreatePresentationConnectionRequest(
      RenderFrameHostId(0, 0), presentation_url, GURL("http://google.com"),
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks))));
  CreateMediaRouterUI(&profile_);

  // Send a sink to the UI that is compatible with sources other than the
  // presentation url to cause a NotFoundError.
  std::vector<MediaSink> sinks;
  sinks.emplace_back("sink id", "sink name", MediaSink::GENERIC);
  std::vector<GURL> origins;
  for (auto* observer : media_sinks_observers_) {
    if (observer->source().id() != presentation_url) {
      observer->OnSinksUpdated(sinks, origins);
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}

TEST_F(MediaRouterUITest, AbortErrorOnClose) {
  content::PresentationError expected_error(
      content::PresentationErrorType::
          PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "Dialog closed.");
  PresentationRequestCallbacks request_callbacks(expected_error);
  std::string presentation_url("http://google.com/presentation");
  create_session_request_.reset(new CreatePresentationConnectionRequest(
      RenderFrameHostId(0, 0), presentation_url, GURL("http://google.com"),
      base::Bind(&PresentationRequestCallbacks::Success,
                 base::Unretained(&request_callbacks)),
      base::Bind(&PresentationRequestCallbacks::Error,
                 base::Unretained(&request_callbacks))));
  CreateMediaRouterUI(&profile_);

  // Send a sink to the UI that is compatible with the presentation url to avoid
  // a NotFoundError.
  std::vector<MediaSink> sinks;
  sinks.emplace_back("sink id", "sink name", MediaSink::GENERIC);
  std::vector<GURL> origins;
  for (auto* observer : media_sinks_observers_) {
    if (observer->source().id() == presentation_url) {
      observer->OnSinksUpdated(sinks, origins);
    }
  }
  // Destroying the UI should return the expected error from above to the error
  // callback.
  media_router_ui_.reset();
}
}  // namespace media_router
