#ifndef AURControllerSkinConfigurations_h
#define AURControllerSkinConfigurations_h

#import <Foundation/Foundation.h>

typedef NS_OPTIONS(int16_t, AURControllerSkinConfigurations)
{
    AURControllerSkinConfigurationIPhoneStandardPortrait    = 1 << 0,
    AURControllerSkinConfigurationIPhoneStandardLandscape   = 1 << 1,
    AURControllerSkinConfigurationIPhoneEdgeToEdgePortrait  = 1 << 4,
    AURControllerSkinConfigurationIPhoneEdgeToEdgeLandscape = 1 << 5,

    AURControllerSkinConfigurationIPadStandardPortrait      = 1 << 6,
    AURControllerSkinConfigurationIPadStandardLandscape     = 1 << 7,
    AURControllerSkinConfigurationIPadSplitViewPortrait     = 1 << 2,
    AURControllerSkinConfigurationIPadSplitViewLandscape    = 1 << 3,
    AURControllerSkinConfigurationIPadEdgeToEdgePortrait    = 1 << 8,
    AURControllerSkinConfigurationIPadEdgeToEdgeLandscape   = 1 << 9,

    AURControllerSkinConfigurationTVStandardPortrait        = 1 << 10,
    AURControllerSkinConfigurationTVStandardLandscape       = 1 << 11,
};

#endif
