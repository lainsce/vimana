#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

extern "C" char *cogito_macos_list_font_names(void) {
  @autoreleasepool {
    CFArrayRef families_cf = CTFontManagerCopyAvailableFontFamilyNames();
    if (!families_cf) return NULL;

    NSArray *families = [(__bridge NSArray *)families_cf
        sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];

    NSMutableString *joined = [NSMutableString string];
    NSString *prev = nil;
    for (NSString *family in families) {
      if (![family isKindOfClass:[NSString class]]) continue;
      NSString *trimmed = [family stringByTrimmingCharactersInSet:
          [NSCharacterSet whitespaceAndNewlineCharacterSet]];
      if (!trimmed || trimmed.length == 0) continue;
      if (prev && [prev caseInsensitiveCompare:trimmed] == NSOrderedSame) continue;
      if (joined.length > 0) [joined appendString:@"\n"];
      [joined appendString:trimmed];
      prev = trimmed;
    }

    CFRelease(families_cf);
    const char *utf8 = [joined UTF8String];
    if (!utf8 || !utf8[0]) return NULL;
    size_t len = strlen(utf8);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, utf8, len + 1);
    return out;
  }
}

extern "C" char *cogito_macos_choose_font_name(const char *current_name) {
  @autoreleasepool {
    CFArrayRef families_cf = CTFontManagerCopyAvailableFontFamilyNames();
    if (!families_cf) return NULL;

    NSArray *families = [(__bridge NSArray *)families_cf
        sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Choose Font";
    alert.informativeText = @"Select a font family to preview in the typography page.";
    [alert addButtonWithTitle:@"Choose"];
    [alert addButtonWithTitle:@"Cancel"];

    NSComboBox *combo = [[NSComboBox alloc] initWithFrame:NSMakeRect(0, 0, 320, 28)];
    [combo setUsesDataSource:NO];
    [combo setCompletes:YES];
    [combo setEditable:YES];
    [combo addItemsWithObjectValues:families];

    NSString *current = nil;
    if (current_name && current_name[0]) {
      current = [NSString stringWithUTF8String:current_name];
    }
    if (current && current.length > 0) {
      [combo setStringValue:current];
    } else if (families.count > 0) {
      [combo setStringValue:families[0]];
    }

    alert.accessoryView = combo;
    [NSApp activateIgnoringOtherApps:YES];

    NSModalResponse response = [alert runModal];
    CFRelease(families_cf);
    if (response != NSAlertFirstButtonReturn) {
      return NULL;
    }

    NSString *chosen = [[combo stringValue] stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!chosen || chosen.length == 0) {
      return NULL;
    }
    const char *utf8 = [chosen UTF8String];
    if (!utf8 || !utf8[0]) {
      return NULL;
    }
    size_t len = strlen(utf8);
    char *out = (char *)malloc(len + 1);
    if (!out) {
      return NULL;
    }
    memcpy(out, utf8, len + 1);
    return out;
  }
}