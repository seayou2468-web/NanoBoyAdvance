//
//  CheatsManager.h
//  Cytrus
//
//  Created by Jarrod Norwell on 19/10/2024.
//  Copyright © 2024 Jarrod Norwell. All rights reserved.
//

#import <Foundation/Foundation.h>

#ifdef __cplusplus
#include <algorithm>
#include <memory>
#include <utility>

#include "core/cheats/cheats.h"
#endif

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_SENDABLE
@interface Cheat : NSObject
@property (nonatomic, assign) BOOL enabled;
@property (nonatomic, strong) NSString *name, *code, *comments;

-(Cheat *) initWithEnabled:(BOOL)enabled name:(NSString *)name code:(NSString *)code comments:(NSString *)comments;
@end

@interface CheatsManager : NSObject {
    uint64_t _identifier;
}

-(CheatsManager *) initWithIdentifier:(uint64_t)identifier;

-(void) loadCheats;
-(void) saveCheats;

-(NSArray<Cheat *> *) getCheats;

-(void) removeCheatAtIndex:(NSInteger)index;
-(void) toggleCheat:(Cheat *)cheat;
-(void) updateCheat:(Cheat *)cheat atIndex:(NSInteger)index;
//Manic修改 获取作弊文件的路径
-(NSString *) cheatFilePath;
@end

NS_ASSUME_NONNULL_END
