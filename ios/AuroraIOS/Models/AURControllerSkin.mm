#import "AURControllerSkin.h"

@implementation AURControllerSkin

- (BOOL)supportsTraits:(AURControllerSkinTraits *)traits {
    AURControllerSkinConfigurations config = 0;
    switch (traits.device) {
        case AURControllerSkinDeviceIPhone:
            if (traits.displayType == AURControllerSkinDisplayTypeEdgeToEdge) {
                config = (traits.orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPhoneEdgeToEdgeLandscape : AURControllerSkinConfigurationIPhoneEdgeToEdgePortrait;
            } else {
                config = (traits.orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPhoneStandardLandscape : AURControllerSkinConfigurationIPhoneStandardPortrait;
            }
            break;
        case AURControllerSkinDeviceIPad:
            // IPad logic simplified
            config = (traits.orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPadStandardLandscape : AURControllerSkinConfigurationIPadStandardPortrait;
            break;
        default: break;
    }
    return (self.supportedConfigurations & config) != 0;
}

@end
