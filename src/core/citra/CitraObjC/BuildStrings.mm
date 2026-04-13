//
//  BuildStrings.mm
//  Cytrus
//
//  Created by Jarrod Norwell on 15/3/2025.
//  Copyright © 2025 Jarrod Norwell. All rights reserved.
//

#import "BuildStrings.h"

#import <Foundation/Foundation.h>

const char* buildDate(void) {
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:0];//Manic修改
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setDateStyle:NSDateFormatterMediumStyle];
    [formatter setTimeStyle:NSDateFormatterShortStyle];
    return [[formatter stringFromDate:date] UTF8String];
}

const char* buildRevision(void) {
    return [@"24a341e925fe75ddab2e596a0ee3fe0ecd93c810" UTF8String];//Manic修改
}

const char* buildFullName(void) {
    NSString *version = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
    NSString *build = [[[NSBundle mainBundle] infoDictionary] objectForKey:(NSString *)kCFBundleVersionKey];
    return [[NSString stringWithFormat:@"%@.%@", version, build] UTF8String];
}
