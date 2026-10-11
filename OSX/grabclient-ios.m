/* xscreensaver, Copyright © 1992-2026 Jamie Zawinski <jwz@jwz.org>
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

  PHFetchOptions *fopt = [[PHFetchOptions new] autorelease];
  fopt.includeAssetSourceTypes = (PHAssetSourceTypeUserLibrary |
                                  PHAssetSourceTypeCloudShared |
                                  PHAssetSourceTypeiTunesSynced);
  PHFetchResult *r = [PHAsset
                       fetchAssetsWithMediaType: PHAssetMediaTypeImage
                       options: fopt];
  NSUInteger n = [r count];
  PHAsset *asset = n ? [r objectAtIndex: random() % n] : NULL;

  if (!asset) {
    // No images; complete immediately.
    callback (0, 0, 0, 0, closure);
    return;
  }

  // Get the image bits, asynchronously.
  //
  PHImageRequestOptions *ropt = [[PHImageRequestOptions new] autorelease];
  ropt.networkAccessAllowed = YES;
  ropt.synchronous  = NO;
  ropt.resizeMode   = PHImageRequestOptionsResizeModeNone;
  ropt.deliveryMode = PHImageRequestOptionsDeliveryModeHighQualityFormat;

  [[PHImageManager defaultManager]
    requestImageForAsset: asset
    targetSize: CGSizeMake (width, height)
    contentMode: PHImageContentModeAspectFit
    options: ropt
    resultHandler:^void (UIImage *img, NSDictionary *info1) {

      if (!img) {
        // Image load failed; maybe it couldn't download from iCloud?
        callback (0, 0, 0, 0, closure);
        return;
      }

      img = [img retain];  // Released in callback below.

      // Get the image name, also asynchronously.
      //
      [asset requestContentEditingInputWithOptions:
               [PHContentEditingInputRequestOptions new]
             completionHandler:^(PHContentEditingInput *ei,
                                 NSDictionary *info2) {
          NSURL *url = ei.fullSizeImageURL;
          const char *fn = 0;
          if (url)
            fn = [[[url lastPathComponent] stringByDeletingPathExtension]
                   cStringUsingEncoding:NSUTF8StringEncoding];

          // Finally, after two trips back to the event loop, we can run
          // the callback.
          //
          callback (img, fn, [img size].width, [img size].height, closure);
          [img release];
     }];

  }];
}

#endif  // HAVE_IPHONE - whole file
