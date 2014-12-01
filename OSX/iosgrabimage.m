/* xscreensaver, Copyright (c) 1992-2014 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* This iOS code to choose and return a random image from the user's
 * photo gallery.
 *
 * Much of the following written by:
 *
 *  Created by David Oster on 6/23/12.
 *  Copyright (c) 2012 Google. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef USE_IPHONE  // whole file

#import <AssetsLibrary/AssetsLibrary.h>
#import "grabscreen.h"
#import "yarandom.h"

/* ALAssetsLibrary is an async API, so we need to fire it off and then
   call a callback when it's done.  Fortunately, this fits the same
   interaction model already used in xscreensaver by load_image_async(),
   so it works out nicely.
 */

typedef struct {
  void (*callback) (void *uiimage, const char *fn, int width, int height,
                    void *closure);
  void *closure;

  ALAssetsLibrary *library;
  NSMutableArray *assets;

} ios_loader_data;


static void
ios_random_image_done (ios_loader_data *d, BOOL ok)
{
  UIImage *img = 0;
  const char *fn = 0;
  NSUInteger n = ok ? [d->assets count] : 0;
  if (n > 0) {
    ALAsset *asset = [d->assets objectAtIndex: random() % n];
    ALAssetRepresentation *rep = [asset defaultRepresentation];

    // "fullScreenImage" returns a smaller image than "fullResolutionImage",
    // but this function still takes a significant fraction of a second,
    // causing a visible glitch in e.g. "glslideshow".
    CGImageRef cgi = [rep fullScreenImage];
    if (cgi) {
      UIImageOrientation orient = (UIImageOrientation) 
        [[asset valueForProperty:ALAssetPropertyOrientation] intValue];
      img = [UIImage imageWithCGImage: cgi
                     scale: 1
                     orientation: orient];
      if (img)
        fn = [[rep filename] cStringUsingEncoding:NSUTF8StringEncoding];
    }
  }

  [d->assets release];
  [d->library release];

  d->callback (img, fn, [img size].width, [img size].height, d->closure);
  free (d);
}


void
ios_load_random_image (void (*callback) (void *uiimage, const char *fn,
                                         int width, int height,
                                         void *closure),
                       void *closure)
{
  ios_loader_data *d = (ios_loader_data *) calloc (1, sizeof(*d));
  d->callback = callback;
  d->closure = closure;

  d->library = [[[ALAssetsLibrary alloc] init] retain];
  d->assets = [[NSMutableArray array] retain];

  // The closures passed in here are called later, after we have returned.
  //
  [d->library enumerateGroupsWithTypes: ALAssetsGroupAll
              usingBlock: ^(ALAssetsGroup *group, BOOL *stop) {
    NSString *name = [group valueForProperty:ALAssetsGroupPropertyName];
    if ([name length]) {
      [group enumerateAssetsUsingBlock: ^(ALAsset *asset, NSUInteger index,
                                          BOOL *stop) {
        if ([[asset valueForProperty: ALAssetPropertyType]
              isEqual: ALAssetTypePhoto]) {
          [d->assets addObject:asset];
        }
      }];
    }

    if (! group) {   // done
      ios_random_image_done (d, YES);
    }
  } failureBlock:^(NSError *error) {
    // E.g., ALAssetsLibraryErrorDomain: "The user has denied the
    // application access to their media."
    NSLog(@"reading Photo Library: %@", error);
    ios_random_image_done (d, NO);
  }];
}

#endif  // USE_IPHONE - whole file
