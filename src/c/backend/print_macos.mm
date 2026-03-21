#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

// Print arbitrary pixel data (e.g. an SDL surface screenshot) via the native
// macOS print dialog.  The caller provides RGBA pixels, width, height.

extern "C" bool cogito_macos_print_image(const unsigned char *pixels,
                                          int width, int height) {
  if (!pixels || width <= 0 || height <= 0) return false;

  @autoreleasepool {
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:width
                      pixelsHigh:height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat:0  // premultiplied alpha
                     bytesPerRow:width * 4
                    bitsPerPixel:32];
    if (!rep) return false;

    memcpy([rep bitmapData], pixels, (size_t)(width * height * 4));

    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [image addRepresentation:rep];

    NSImageView *view = [[NSImageView alloc]
        initWithFrame:NSMakeRect(0, 0, width, height)];
    [view setImage:image];
    [view setImageScaling:NSImageScaleProportionallyUpOrDown];

    NSPrintInfo *info = [NSPrintInfo sharedPrintInfo];
    [info setHorizontalPagination:NSPrintingPaginationModeFit];
    [info setVerticalPagination:NSPrintingPaginationModeFit];
    [info setOrientation:(width > height) ? NSPaperOrientationLandscape
                                          : NSPaperOrientationPortrait];

    NSPrintOperation *op = [NSPrintOperation printOperationWithView:view
                                                          printInfo:info];
    [op setShowsPrintPanel:YES];
    [op setShowsProgressPanel:YES];

    return [op runOperation] ? true : false;
  }
}

// Print a text string via the native macOS print dialog.
extern "C" bool cogito_macos_print_text(const char *text) {
  if (!text || !text[0]) return false;

  @autoreleasepool {
    NSString *str = [NSString stringWithUTF8String:text];
    if (!str) return false;

    NSDictionary *attrs = @{
      NSFontAttributeName : [NSFont systemFontOfSize:12.0],
      NSForegroundColorAttributeName : [NSColor textColor]
    };
    NSAttributedString *attrStr =
        [[NSAttributedString alloc] initWithString:str attributes:attrs];

    // Create a text view sized to fit the content
    NSTextView *view = [[NSTextView alloc]
        initWithFrame:NSMakeRect(0, 0, 612, 792)]; // US Letter
    [[view textStorage] setAttributedString:attrStr];

    NSPrintInfo *info = [NSPrintInfo sharedPrintInfo];
    [info setHorizontalPagination:NSPrintingPaginationModeFit];
    [info setVerticalPagination:NSPrintingPaginationModeAutomatic];

    NSPrintOperation *op = [NSPrintOperation printOperationWithView:view
                                                          printInfo:info];
    [op setShowsPrintPanel:YES];
    [op setShowsProgressPanel:YES];

    return [op runOperation] ? true : false;
  }
}
