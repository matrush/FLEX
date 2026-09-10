//
//  PTDatabaseManager.h
//  Derived from:
//
//  FMDatabase.h
//  FMDB( https://github.com/ccgus/fmdb )
//
//  Created by Peng Tao on 15/11/23.
//
//  Licensed to Flying Meat Inc. under one or more contributor license agreements.
//  See the LICENSE file distributed with this work for the terms under
//  which Flying Meat Inc. licenses this file to you.

#import <Foundation/Foundation.h>
#import "FLEXSQLResult.h"

/// Conformers should automatically open and close the database
@protocol FLEXDatabaseManager <NSObject>

@required

/// @return \c nil if the database couldn't be opened
+ (instancetype)managerForDatabase:(NSString *)path;

/// @return a list of all table names
- (NSArray<NSString *> *)queryAllTables;
- (NSArray<NSString *> *)queryAllColumnsOfTable:(NSString *)tableName;
- (NSArray<NSArray *> *)queryAllDataInTable:(NSString *)tableName;

@optional

- (NSArray<NSString *> *)queryRowIDsInTable:(NSString *)tableName;
- (FLEXSQLResult *)executeStatement:(NSString *)SQLStatement;

/// @return the total number of rows in the given table
- (NSInteger)rowCountInTable:(NSString *)tableName;
/// Paged variant of \c queryAllDataInTable:
- (NSArray<NSArray *> *)queryDataInTable:(NSString *)tableName limit:(NSInteger)limit offset:(NSInteger)offset;
/// Paged variant of \c queryRowIDsInTable:
- (NSArray<NSString *> *)queryRowIDsInTable:(NSString *)tableName limit:(NSInteger)limit offset:(NSInteger)offset;

@end
