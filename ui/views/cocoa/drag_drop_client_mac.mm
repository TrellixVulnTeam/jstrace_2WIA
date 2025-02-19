// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/drag_drop_client_mac.h"

#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/views/drag_utils.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/widget/native_widget_mac.h"

@interface CocoaDragDropDataProvider ()
- (id)initWithData:(const ui::OSExchangeData&)data;
- (id)initWithPasteboard:(NSPasteboard*)pasteboard;
@end

@implementation CocoaDragDropDataProvider {
  std::unique_ptr<ui::OSExchangeData> data_;
}

- (id)initWithData:(const ui::OSExchangeData&)data {
  if ((self = [super init])) {
    data_.reset(new OSExchangeData(
        std::unique_ptr<OSExchangeData::Provider>(data.provider().Clone())));
  }
  return self;
}

- (id)initWithPasteboard:(NSPasteboard*)pasteboard {
  if ((self = [super init])) {
    data_ = ui::OSExchangeDataProviderMac::CreateDataFromPasteboard(pasteboard);
  }
  return self;
}

- (ui::OSExchangeData*)data {
  return data_.get();
}

// NSPasteboardItemDataProvider protocol implementation.

- (void)pasteboard:(NSPasteboard*)sender
                  item:(NSPasteboardItem*)item
    provideDataForType:(NSString*)type {
  const ui::OSExchangeDataProviderMac& provider =
      static_cast<const ui::OSExchangeDataProviderMac&>(data_->provider());
  NSData* ns_data = provider.GetNSDataForType(type);
  [sender setData:ns_data forType:type];
}

@end

namespace views {

DragDropClientMac::DragDropClientMac(BridgedNativeWidget* bridge,
                                     View* root_view)
    : drop_helper_(root_view),
      operation_(0),
      bridge_(bridge),
      quit_closure_(base::Closure()) {
  DCHECK(bridge);
}

DragDropClientMac::~DragDropClientMac() {}

void DragDropClientMac::StartDragAndDrop(
    View* view,
    const ui::OSExchangeData& data,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  data_source_.reset([[CocoaDragDropDataProvider alloc] initWithData:data]);
  operation_ = operation;

  const ui::OSExchangeDataProviderMac& provider =
      static_cast<const ui::OSExchangeDataProviderMac&>(data.provider());

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = bridge_->ns_window();
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval event_time = [[NSApp currentEvent] timestamp];
  NSEvent* event = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                      location:position
                                 modifierFlags:NSLeftMouseDraggedMask
                                     timestamp:event_time
                                  windowNumber:[window windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:1.0];

  NSImage* image = gfx::NSImageFromImageSkiaWithColorSpace(
      provider.GetDragImage(), base::mac::GetSRGBColorSpace());

  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);
  [item setDataProvider:data_source_.get()
               forTypes:ui::OSExchangeDataProviderMac::
                            SupportedPasteboardTypes()];

  base::scoped_nsobject<NSDraggingItem> drag_item(
      [[NSDraggingItem alloc] initWithPasteboardWriter:item.get()]);

  // Subtract the image's height from the y location so that the mouse will be
  // at the upper left corner of the image.
  NSRect dragging_frame =
      NSMakeRect([event locationInWindow].x,
                 [event locationInWindow].y - [image size].height,
                 [image size].width, [image size].height);
  [drag_item setDraggingFrame:dragging_frame contents:image];

  [bridge_->ns_view() beginDraggingSessionWithItems:@[ drag_item.get() ]
                                              event:event
                                             source:bridge_->ns_view()];

  // Since Drag and drop is asynchronous on Mac, we need to spin a nested run
  // loop for consistency with other platforms.
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

NSDragOperation DragDropClientMac::DragUpdate(id<NSDraggingInfo> sender) {
  int drag_operation = ui::DragDropTypes::DRAG_NONE;

  // Since dragging from non MacView sources does not generate OSExchangeData,
  // we need to generate one based on the provided pasteboard.
  if (!data_source_.get()) {
    data_source_.reset([[CocoaDragDropDataProvider alloc]
        initWithPasteboard:[sender draggingPasteboard]]);
  }

  drag_operation = drop_helper_.OnDragOver(
      *[data_source_ data], LocationInView([sender draggingLocation]),
      operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(drag_operation);
}

NSDragOperation DragDropClientMac::Drop(id<NSDraggingInfo> sender) {
  int drag_operation = drop_helper_.OnDrop(
      *[data_source_ data], LocationInView([sender draggingLocation]),
      operation_);
  return ui::DragDropTypes::DragOperationToNSDragOperation(drag_operation);
}

void DragDropClientMac::EndDrag() {
  data_source_.reset();

  // Allow a test to invoke EndDrag() without spinning the nested run loop.
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
    quit_closure_.Reset();
  }
}

gfx::Point DragDropClientMac::LocationInView(NSPoint point) const {
  return gfx::Point(point.x, NSHeight([bridge_->ns_window() frame]) - point.y);
}

}  // namespace views
