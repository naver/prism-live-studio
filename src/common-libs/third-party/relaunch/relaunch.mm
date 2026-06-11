#import <Cocoa/Cocoa.h>

int main(int argc, char *argv[])
{
	pid_t parentPID = atoi(argv[2]);

	while ([NSRunningApplication runningApplicationWithProcessIdentifier:parentPID]) {
		sleep(2);
	}
    sleep(1);
    NSMutableArray *argument = [[NSMutableArray alloc] init];
    for(int i =0; i < argc; i++) {
        char *argu = argv[i];
        [argument addObject:[NSString stringWithCString:argu encoding:NSUTF8StringEncoding]];
    }
	NSString *appPath = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:appPath];
    [task setArguments:[argument copy]];
    NSError *error = NULL;
    [task launchAndReturnError:&error];
    if (error) {
        return 1;
    }
    return 0;
}
