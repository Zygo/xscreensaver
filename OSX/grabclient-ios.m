/* xscreensaver, Copyright (c) 1992-2019 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* iOS 8+ code to choose and return a random image from the photo library.
 */

#ifdef HAVE_IPHONE  // whole file

#import <Photos/Photos.h>
#import "grabclient.h"
#import "yarandom.h"

void
ios_load_random_image (void (*callback) (void *uiimage, const char *fn,
                                         int width, int height,
                                         void *closure),
                       void *closure,
                       int width, int height)
{
  // If the user has not yet been asked for authoriziation, pop up the
  // auth dialog now and re-invoke this function once it has been
  // answered.  The callback will run once there has been a Yes or No.
  // Otherwise, we'd return right away with colorbars even if the user
  // then went on to answer Yes.
  //
  PHAuthorizationStatus status = [PHPhotoLibrary authorizationStatus];
  if (status == PHAuthorizationStatusNotDetermined) {
    [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
      ios_load_random_image (callback, closure, width, height);
    }];
    return;
  }

  // The rest of this is synchronous.

  PHFetchOptions *opt = [[PHFetchOptions new] autorelease];
  opt.includeAssetSourceTypes = (PHAssetSourceTypeUserLibrary |
                                 PHAssetSourceTypeCloudShared |
                                 PHAssetSourceTypeiTunesSynced);
  PHFetchResult *r = [PHAsset
                       fetchAssetsWithMediaType: PHAssetMediaTypeImage
                       options: opt];
  NSUInteger n = [r count];
  PHAsset *asset = n ? [r objectAtIndex: random() % n] : NULL;

  __block UIImage *img = 0;
  __block const char *fn = 0;

  if (asset) {
    PHImageRequestOptions *opt = [[PHImageRequestOptions alloc] init];
    opt.synchronous = YES;

    // Get the image bits.
    //
    int size = width > height ? width : height;
    [[PHImageManager defaultManager]
      requestImageForAsset: asset
      targetSize: CGSizeMake (size, size)
      contentMode: PHImageContentModeDefault
      options: opt
      resultHandler:^void (UIImage *image, NSDictionary *info) {
        img = [image retain];
    }];

    // Get the image name.
    //
    [[PHImageManager defaultManager]
      requestImageDataForAsset: asset
      options: opt
      resultHandler:^(NSData *imageData, NSString *dataUTI,
                      UIImageOrientation orientation, 
                      NSDictionary *info) {
        // Looks like UIImage is pre-rotated to compensate for 'orientation'.
        NSString *path = [info objectForKey:@"PHImageFileURLKey"];
        if (path)
          fn = [[[path lastPathComponent] stringByDeletingPathExtension]
                 cStringUsingEncoding:NSUTF8StringEncoding];
    }];
  }

  if (img)
    callback (img, fn, [img size].width, [img size].height, closure);
  else
    callback (0, 0, 0, 0, closure);
}

#endif  // HAVE_IPHONE - whole file
