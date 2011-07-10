//
//  HRV_tracker_OSXAppDelegate.h
//  HRV_tracker_OSX
//
//  Created by STAR DEVELOPMENT on 8/07/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface HRV_tracker_OSXAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
	NSButton *btnDeviceHelp;
	NSButton *btnManualStart;
	NSButton *btnManualStop;
	NSTextField *txtDevice;
	NSTextField *txtHR;
	NSTextField *txtRR;
	NSTextField *txtStatus;
	NSMutableArray *arrayRR;
}

@property (assign) IBOutlet NSWindow *window;
@property (nonatomic, retain) IBOutlet NSButton *btnDeviceHelp;
@property (nonatomic, retain) IBOutlet NSButton *btnManualStart;
@property (nonatomic, retain) IBOutlet NSButton *btnManualStop;
@property (nonatomic, retain) IBOutlet NSTextField *txtDevice;
@property (nonatomic, retain) IBOutlet NSTextField *txtHR;
@property (nonatomic, retain) IBOutlet NSTextField *txtRR;
@property (nonatomic, retain) IBOutlet NSTextField *txtStatus;
@property (nonatomic, retain) NSMutableArray *arrayRR;

-(IBAction) showDeviceHelp:(id) sender;
-(IBAction) startManualRecording: (id) sender;
-(IBAction) stopManualRecording: (id) sender;
-(void) hrThread: (id) threadObject;
-(void) updateDisplay;

@end
