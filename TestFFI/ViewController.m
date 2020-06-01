//
//  ViewController.m
//  TestFFI
//
//  Created by micro on 2020/4/27.
//  Copyright © 2020 micro. All rights reserved.
//

#import "ViewController.h"
#import "TestCase.h"
#import "TestFFI.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
}

- (IBAction)OnTestFFI:(id)sender {
    [TestFFI Run];
}

- (IBAction)OnRunSimpleFFITestCase:(id)sender {
    [TestCase Run];
}

@end
