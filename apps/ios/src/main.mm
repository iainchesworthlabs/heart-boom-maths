// SPDX-License-Identifier: GPL-3.0-or-later
#import <UIKit/UIKit.h>

#include <string>

#include "heart_boom_maths/version.hpp"

// The smallest application that can be signed, uploaded to TestFlight and opened: one screen showing the name and the
// version of the game. It stays until the Qt build of apps/game runs on iOS (milestone M2 decides how Qt is built for
// it); what stays after that is the packaging around it (CMakeLists.txt, packaging/) and the workflows.

@interface IclAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation IclAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    (void)application;
    (void)launchOptions;

    UIViewController* controller = [[UIViewController alloc] init];
    controller.view.backgroundColor = UIColor.systemBackgroundColor;

    const std::string version(heart_boom_maths::VERSION_STRING);
    UILabel* label = [[UILabel alloc] init];
    label.text = [NSString stringWithFormat:@"Heart Boom Maths\n%s", version.c_str()];
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    label.adjustsFontForContentSizeCategory = YES;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [controller.view addSubview:label];
    // The label stays inside the safe area (the notch, the home indicator, the rounded corners).
    UILayoutGuide* safe = controller.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [label.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
        [label.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:24],
        [label.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-24],
    ]];

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main(int argc, char* argv[])
{
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([IclAppDelegate class]));
    }
}
