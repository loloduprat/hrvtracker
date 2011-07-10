//
//  HRV_tracker_OSXAppDelegate.m
//  HRV_tracker_OSX
//
//  Created by STAR DEVELOPMENT on 8/07/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "HRV_tracker_OSXAppDelegate.h"

#include "libant.h"

@implementation HRV_tracker_OSXAppDelegate

@synthesize window;
@synthesize btnDeviceHelp;
@synthesize btnManualStart;
@synthesize btnManualStop;
@synthesize txtDevice;
@synthesize txtHR;
@synthesize txtRR;
@synthesize txtStatus;
@synthesize arrayRR;

int isRecording = 0;
struct hr {
	u8 page;
	u8 u1;
	u16 u2;
	u16 rr;
	u8 seq;
	u8 hr;
} *hrp;
struct rcvmsg *rm;
int seen[MAXCHAN];
int lastseq[MAXCHAN];
int lastrr[MAXCHAN];	
struct anth *h = 0;
int hr = -1;
int rr = -1;


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	// Insert code here to initialize your application 
}


-(void) hrThread: (id) threadObject {
	for (;;) {
	    //Thread to wait for new HR data to arrive
	    rm = ant_getdata(h, &isRecording);
	    //if (dbg) fprintf(stderr, "got data %p\n", rm);
	    hrp = (struct hr *)rm->data;
	    if (!seen[rm->chan]) {
     		 msg_send(h, MESG_REQUEST_ID, rm->chan, 0x51);
	    	 seen[rm->chan] = 1;
		     lastseq[rm->chan] = hrp->seq;
		    lastrr[rm->chan] = hrp->rr;
	    } else {
		    if (hrp->seq != lastseq[rm->chan]) {
				hr = hrp->hr;
				rr = hrp->rr-lastrr[rm->chan];
				[self performSelectorOnMainThread:@selector(updateDisplay) withObject:nil waitUntilDone:FALSE];
		    }
		    //fprintf(stderr, "HR ch#%d: %d rr: %d diff: %d seq %d\n", rm->chan, hrp->hr, hrp->rr, hrp->rr-lastrr[rm->chan], hrp->seq);
		    lastrr[rm->chan] = hrp->rr;
		    lastseq[rm->chan] = hrp->seq;
    	 }
		if (isRecording == 0) {
			break;
		}
	}
}

-(void) updateDisplay {
	//Numerical wrapping of the RR interval counter (around 32,768)
	int rr_new = rr;
	if (rr_new < 0) {
		rr_new = rr_new + 32768;
	}
	//Display this HR and RR interval
	[txtHR setStringValue:[NSString stringWithFormat:@"HR: %d BPM", hr]];
	[txtRR setStringValue:[NSString stringWithFormat:@"RR: %d ms", rr_new]];
	//Add to the list of RR intervals to save
	[arrayRR addObject:[NSString stringWithFormat:@"%d", rr_new]];
	
}

- (IBAction) showDeviceHelp: (id) sender {
	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert addButtonWithTitle:@"OK"];
	[alert setMessageText:@"Help"];
	[alert setInformativeText:@"Garmin USB1 devices are usually available on:\n\n/dev/cu.ANTUSBStick.slabvcp\n\nGarmin USB2 devices have a diffrent device folder"];
	[alert setAlertStyle:NSInformationalAlertStyle];
	[alert runModal];
}

- (IBAction) startManualRecording: (id) sender {
	NSString *devfile;
	devfile = [txtDevice stringValue];
	unsigned speed = 115200;
	int rtscts = 0;
	int srchto = 10;



	
	// config
	ant_debug(0);
	ant_errors(ANT_EXIT);
	ant_messages(ANT_PRINT);
	ant_sync(1);
	
	// start
	if (h != 0) {
		//free(h);
	}
	h = ant_open((char *)[devfile UTF8String], speed, rtscts);
	
	// setup
	ant_reset(h);
	ant_req_cap(h);
	
	
	msg_send(h, MESG_NETWORK_KEY_ID, 0, "b9a521fbbd72c345"); // ANT+
	msg_send(h, MESG_NETWORK_KEY_ID, 1, "b9ad3228757ec74d"); // Suunto
	
	msg_send(h, MESG_ASSIGN_CHANNEL_ID, 0, 0, 0);
	msg_send(h, MESG_CHANNEL_ID_ID, 0, 0, 0, 0);
	msg_send(h, MESG_CHANNEL_SEARCH_TIMEOUT_ID, 0, srchto);
	msg_send(h, MESG_CHANNEL_MESG_PERIOD_ID, 0, 0x1f86);
	msg_send(h, MESG_CHANNEL_RADIO_FREQ_ID, 0, 0x39);
	
	msg_send(h, MESG_ASSIGN_CHANNEL_ID, 1, 0, 0);
	msg_send(h, MESG_CHANNEL_ID_ID, 1, 1, 1, 0);
	msg_send(h, MESG_CHANNEL_SEARCH_TIMEOUT_ID, 1, srchto);
	msg_send(h, MESG_CHANNEL_MESG_PERIOD_ID, 1, 0x1f86);
	msg_send(h, MESG_CHANNEL_RADIO_FREQ_ID, 1, 0x39);
	
	msg_send(h, MESG_ASSIGN_CHANNEL_ID, 2, 0, 1);
	msg_send(h, MESG_CHANNEL_ID_ID, 2, 2, 2, 0);
	msg_send(h, MESG_CHANNEL_SEARCH_TIMEOUT_ID, 2, srchto);
	msg_send(h, MESG_CHANNEL_MESG_PERIOD_ID, 2, 0x199a);
	msg_send(h, MESG_CHANNEL_RADIO_FREQ_ID, 2, 0x41);
	
	msg_send(h, MESG_ASSIGN_CHANNEL_ID, 3, 0, 1);
	msg_send(h, MESG_CHANNEL_ID_ID, 3, 3, 1, 0);
	msg_send(h, MESG_CHANNEL_SEARCH_TIMEOUT_ID, 3, srchto);
	msg_send(h, MESG_CHANNEL_MESG_PERIOD_ID, 3, 0x199a);
	msg_send(h, MESG_CHANNEL_RADIO_FREQ_ID, 3, 0x41);
	
	msg_send(h, MESG_OPEN_CHANNEL_ID, 0);
	msg_send(h, MESG_OPEN_CHANNEL_ID, 1);
	msg_send(h, MESG_OPEN_CHANNEL_ID, 2);
	msg_send(h, MESG_OPEN_CHANNEL_ID, 3);	
	
	//data storage
	arrayRR = [[NSMutableArray array] retain];
	[arrayRR addObject:@"[Params]"];
	[arrayRR addObject:@"Date="];
	[arrayRR addObject:@"StartTime="];
	[arrayRR addObject:@"Length="];
	[arrayRR addObject:@"Interval="];	
	[arrayRR addObject:@""];
	
	
	[arrayRR addObject:@"[HRData]"];	//header used for r-r interval files
	
	isRecording = 1;
	[txtStatus setStringValue:@"Recording..."];
	//start the thread to wait for HR messages
	[NSThread detachNewThreadSelector:@selector(hrThread:) toTarget:self withObject:nil];
	
}

-(IBAction) stopManualRecording: (id) sender {
	[txtStatus setStringValue:@"Ready"];
	isRecording = 0;
	[txtHR setStringValue:@"HR: --"];
	[txtRR setStringValue:@"RR: --"];
	
	NSSavePanel *savePanel;
	savePanel = [NSSavePanel savePanel];
	[savePanel setAllowedFileTypes:[NSArray arrayWithObject:@"hrm"]];
	NSInteger clicked = [savePanel runModal];
	NSMutableString *outputString;
	outputString = [NSMutableString string];
	int i=0;
	if (clicked == NSFileHandlingPanelOKButton) {
		//save the file
		for(i=0; i<[arrayRR count]; i++) {
			[outputString appendString:(NSString *)[arrayRR objectAtIndex:i]];
			[outputString appendString:@"\n"];
		}
		[outputString writeToFile:[[savePanel URL] path] atomically:YES encoding:NSASCIIStringEncoding error:nil];
	}
	[arrayRR release];
}

@end
