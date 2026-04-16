#import "AURGame.h"

@implementation AURGame
+ (BOOL)supportsSecureCoding { return YES; }
- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.title forKey:@"title"];
    [coder encodeObject:self.romPath forKey:@"romPath"];
    [coder encodeInteger:self.coreType forKey:@"coreType"];
    [coder encodeObject:self.boxArtPath forKey:@"boxArtPath"];
}
- (instancetype)initWithCoder:(NSCoder *)coder {
    self = [super init];
    if (self) {
        _title = [coder decodeObjectOfClass:[NSString class] forKey:@"title"];
        _romPath = [coder decodeObjectOfClass:[NSString class] forKey:@"romPath"];
        _coreType = (EmulatorCoreType)[coder decodeIntegerForKey:@"coreType"];
        _boxArtPath = [coder decodeObjectOfClass:[NSString class] forKey:@"boxArtPath"];
    }
    return self;
}
@end
