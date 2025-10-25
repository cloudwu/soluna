#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreText/CoreText.h>
#import <objc/runtime.h>
#import <objc/message.h>

#include <stdint.h>

#include "../../ime_state.h"
#include "soluna_macos_ime.h"
#include "sokol/sokol_app.h"

@interface _sapp_macos_view : NSView
@end

// Forward declarations implemented in entry.c
void soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat);

static bool g_soluna_macos_composition = false;
static NSTextField *g_soluna_ime_label = nil;
static NSString *g_soluna_macos_ime_font_name = nil;
static CGFloat g_soluna_macos_ime_font_size = 14.0f;
static NSView *g_soluna_ime_caret = nil;

static NSString *soluna_current_marked_text(NSView *view);
static NSRange soluna_current_selected_range(NSView *view);

static void
soluna_macos_apply_ime_font(void) {
    if (!g_soluna_ime_label) {
        return;
    }
    CGFloat size = g_soluna_macos_ime_font_size > 0.0f ? g_soluna_macos_ime_font_size : 14.0f;
    NSFont *font = nil;
    if (g_soluna_macos_ime_font_name) {
        font = [NSFont fontWithName:g_soluna_macos_ime_font_name size:size];
    }
    if (font == nil) {
        font = [NSFont systemFontOfSize:size];
    }
    [g_soluna_ime_label setFont:font];
}

void
soluna_macos_set_ime_font(const char *font_name, float height_px) {
    if (g_soluna_macos_ime_font_name) {
        [g_soluna_macos_ime_font_name release];
        g_soluna_macos_ime_font_name = nil;
    }
    if (font_name && font_name[0]) {
        NSString *converted = [[NSString alloc] initWithUTF8String:font_name];
        if (converted) {
            g_soluna_macos_ime_font_name = converted;
        } else {
            [converted release];
        }
    }
    if (height_px > 0.0f) {
        g_soluna_macos_ime_font_size = (CGFloat)height_px;
    } else {
        g_soluna_macos_ime_font_size = 0.0f;
    }
    soluna_macos_apply_ime_font();
}

static NSView *
soluna_macos_ensure_ime_caret(NSView *view) {
    if (g_soluna_ime_caret == nil) {
        g_soluna_ime_caret = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
        [g_soluna_ime_caret setWantsLayer:YES];
        CALayer *layer = [g_soluna_ime_caret layer];
        if (layer) {
            layer.backgroundColor = [[NSColor controlAccentColor] CGColor];
        }
    }
    if (g_soluna_ime_caret.superview != view) {
        [g_soluna_ime_caret removeFromSuperview];
        if (view) {
            NSView *relative = g_soluna_ime_label && g_soluna_ime_label.superview == view ? g_soluna_ime_label : nil;
            [view addSubview:g_soluna_ime_caret positioned:NSWindowAbove relativeTo:relative];
        }
    }
    return g_soluna_ime_caret;
}

static void
soluna_macos_hide_ime_caret(void) {
    if (g_soluna_ime_caret) {
        [g_soluna_ime_caret setHidden:YES];
    }
}

static void
soluna_macos_position_ime_caret(NSView *view, NSTextField *label, NSAttributedString *attr, NSRange selectedRange) {
    if (selectedRange.location == NSNotFound) {
        soluna_macos_hide_ime_caret();
        return;
    }
    NSView *caret = soluna_macos_ensure_ime_caret(view);
    if (selectedRange.length > 0) {
        [caret setHidden:YES];
        return;
    }
    NSUInteger caretIndex = selectedRange.location + selectedRange.length;
    NSUInteger textLength = attr.length;
    if (caretIndex > textLength) {
        caretIndex = textLength;
    }
    NSRect textRect = [[label cell] drawingRectForBounds:label.bounds];
    CGFloat prefixWidth = 0.0f;
    CGFloat lineHeight = 0.0f;
    CTLineRef line = CTLineCreateWithAttributedString((CFAttributedStringRef)attr);
    if (line) {
        if (caretIndex > textLength) {
            caretIndex = textLength;
        }
        double caretOffset = CTLineGetOffsetForStringIndex(line, caretIndex, NULL);
        prefixWidth = (CGFloat)ceil(caretOffset);
        double ascent = 0.0, descent = 0.0, leading = 0.0;
        CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
        lineHeight = (CGFloat)(ascent + descent + leading);
        CFRelease(line);
    }
    NSRect caretFrame;
    caretFrame.size.width = 2.0f;
    if (lineHeight <= 0.0f) {
        NSFont *font = [label font];
        if (font) {
            lineHeight = font.ascender - font.descender;
        }
        if (lineHeight <= 0.0f) {
            lineHeight = g_soluna_macos_ime_font_size > 0.0f ? g_soluna_macos_ime_font_size : 14.0f;
        }
    }
    caretFrame.size.height = MAX(1.0f, lineHeight);
    caretFrame.origin.x = label.frame.origin.x + textRect.origin.x + prefixWidth;
    CGFloat originY = label.frame.origin.y + textRect.origin.y;
    CGFloat verticalAdjust = 0.0f;
    if (textRect.size.height > caretFrame.size.height) {
        verticalAdjust = (textRect.size.height - caretFrame.size.height) * 0.5f;
    }
    caretFrame.origin.y = originY + verticalAdjust;
    [caret setFrame:NSIntegralRect(caretFrame)];
    [caret setHidden:NO];
}

static NSRect
soluna_current_caret_local_rect(NSView *view) {
    NSRect caret = NSMakeRect(0, 0, 1, 1);
    if (g_soluna_ime_rect.valid) {
        CGFloat dpi_scale = sapp_dpi_scale();
        if (dpi_scale <= 0.0f) {
            dpi_scale = 1.0f;
        }
        CGFloat logical_height = (CGFloat)sapp_height() / dpi_scale;
        CGFloat caret_y = logical_height - (g_soluna_ime_rect.y + g_soluna_ime_rect.h);
        caret = NSMakeRect(g_soluna_ime_rect.x, caret_y, g_soluna_ime_rect.w, g_soluna_ime_rect.h);
    }
    return caret;
}

static NSTextField *
soluna_macos_ensure_ime_label(NSView *view) {
    if (g_soluna_ime_label == nil) {
        g_soluna_ime_label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
        [g_soluna_ime_label setEditable:NO];
        [g_soluna_ime_label setSelectable:NO];
        [g_soluna_ime_label setBezeled:NO];
        [g_soluna_ime_label setDrawsBackground:NO];
        [g_soluna_ime_label setBordered:NO];
        [g_soluna_ime_label setBackgroundColor:[NSColor clearColor]];
        [g_soluna_ime_label setHidden:YES];
        [g_soluna_ime_label setLineBreakMode:NSLineBreakByClipping];
        [g_soluna_ime_label setUsesSingleLineMode:YES];
        [g_soluna_ime_label setTranslatesAutoresizingMaskIntoConstraints:YES];
        soluna_macos_apply_ime_font();
    }
    if (g_soluna_ime_label.superview != view) {
        [g_soluna_ime_label removeFromSuperview];
        if (view) {
            [view addSubview:g_soluna_ime_label];
        }
    }
    soluna_macos_apply_ime_font();
    return g_soluna_ime_label;
}

void
soluna_macos_hide_ime_label(void) {
    if (g_soluna_ime_label) {
        [g_soluna_ime_label setHidden:YES];
    }
    soluna_macos_hide_ime_caret();
}

static void
soluna_macos_position_ime_label(NSView *view, NSString *text, NSRange selectedRange) {
    if (text == nil || text.length == 0) {
        soluna_macos_hide_ime_label();
        return;
    }
    NSTextField *label = soluna_macos_ensure_ime_label(view);
    if (label == nil) {
        return;
    }
    NSMutableAttributedString *attr = [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
    NSRange fullRange = NSMakeRange(0, attr.length);
    if (fullRange.length > 0) {
        NSFont *labelFont = [label font];
        if (labelFont) {
            [attr addAttribute:NSFontAttributeName value:labelFont range:fullRange];
        }
        [attr addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:fullRange];
        [attr addAttribute:NSUnderlineStyleAttributeName value:@(NSUnderlineStyleSingle) range:fullRange];
    }
    if (selectedRange.length > 0 && NSMaxRange(selectedRange) <= attr.length) {
        [attr addAttribute:NSBackgroundColorAttributeName value:[NSColor controlAccentColor] range:selectedRange];
        [attr addAttribute:NSForegroundColorAttributeName value:[NSColor alternateSelectedControlTextColor] range:selectedRange];
    }
    [label setAttributedStringValue:attr];
    NSSize textSize = [[label cell] cellSizeForBounds:NSMakeRect(0, 0, CGFLOAT_MAX, CGFLOAT_MAX)];
    CGFloat padding = 6.0f;
    textSize.width += padding;
    textSize.height += padding;
    NSRect caret = soluna_current_caret_local_rect(view);
    CGFloat baseline = caret.origin.y + caret.size.height;
    CGFloat baselineOffset = [label baselineOffsetFromBottom];
    CGFloat frameOriginY = baseline - baselineOffset - textSize.height;
    NSRect frame = NSMakeRect(caret.origin.x, frameOriginY, textSize.width, textSize.height);
    NSRect bounds = view.bounds;
    if (frame.origin.x < bounds.origin.x) {
        frame.origin.x = bounds.origin.x;
    }
    CGFloat maxX = NSMaxX(bounds);
    if (NSMaxX(frame) > maxX) {
        frame.origin.x = maxX - frame.size.width;
    }
    if (frame.origin.y < bounds.origin.y) {
        frame.origin.y = bounds.origin.y;
    }
    CGFloat maxY = NSMaxY(bounds);
    if (NSMaxY(frame) > maxY) {
        frame.origin.y = maxY - frame.size.height;
    }
    [label setFrame:NSIntegralRect(frame)];
    [label setHidden:NO];
    soluna_macos_position_ime_caret(view, label, attr, selectedRange);
}

static void
soluna_macos_refresh_ime_label(NSView *view) {
    if (g_soluna_ime_label == nil || [g_soluna_ime_label isHidden]) {
        return;
    }
    NSString *text = soluna_current_marked_text(view);
    NSRange range = soluna_current_selected_range(view);
    soluna_macos_position_ime_label(view, text, range);
}

static const void *const kSolunaMarkedTextKey = &kSolunaMarkedTextKey;
static const void *const kSolunaSelectedRangeKey = &kSolunaSelectedRangeKey;
static const void *const kSolunaConsumedFlagKey = &kSolunaConsumedFlagKey;

static uint32_t
soluna_modifiers_from_event(NSEvent *event) {
    NSEventModifierFlags flags = event ? event.modifierFlags : NSEvent.modifierFlags;
    uint32_t mods = 0;
    if (flags & NSEventModifierFlagShift) {
        mods |= SAPP_MODIFIER_SHIFT;
    }
    if (flags & NSEventModifierFlagControl) {
        mods |= SAPP_MODIFIER_CTRL;
    }
    if (flags & NSEventModifierFlagOption) {
        mods |= SAPP_MODIFIER_ALT;
    }
    if (flags & NSEventModifierFlagCommand) {
        mods |= SAPP_MODIFIER_SUPER;
    }
    return mods;
}

static uint32_t
soluna_utf32_from_substring(NSString *substr) {
    if (substr == nil || substr.length == 0) {
        return 0;
    }
    unichar buffer[2] = {0};
    NSUInteger len = substr.length;
    [substr getCharacters:buffer range:NSMakeRange(0, len)];
    if (len >= 2 && buffer[0] >= 0xD800 && buffer[0] <= 0xDBFF && buffer[1] >= 0xDC00 && buffer[1] <= 0xDFFF) {
        uint32_t high = buffer[0] - 0xD800;
        uint32_t low = buffer[1] - 0xDC00;
        return (high << 10) + low + 0x10000;
    }
    return buffer[0];
}

static NSString *
soluna_plain_string(id string) {
    if ([string isKindOfClass:[NSAttributedString class]]) {
        return [(NSAttributedString *)string string];
    }
    if ([string isKindOfClass:[NSString class]]) {
        return (NSString *)string;
    }
    return [string description];
}

static void
soluna_store_marked_text(NSView *view, NSString *text, NSRange selected_range) {
    if (text && text.length > 0) {
        objc_setAssociatedObject(view, kSolunaMarkedTextKey, text, OBJC_ASSOCIATION_COPY_NONATOMIC);
        NSValue *value = [NSValue valueWithRange:selected_range];
        objc_setAssociatedObject(view, kSolunaSelectedRangeKey, value, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    } else {
        objc_setAssociatedObject(view, kSolunaMarkedTextKey, nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject(view, kSolunaSelectedRangeKey, nil, OBJC_ASSOCIATION_ASSIGN);
    }
}

static NSString *
soluna_current_marked_text(NSView *view) {
    return objc_getAssociatedObject(view, kSolunaMarkedTextKey);
}

static bool
soluna_view_has_marked_text(NSView *view) {
    NSString *text = soluna_current_marked_text(view);
    return text && text.length > 0;
}

static NSRange
soluna_current_selected_range(NSView *view) {
    NSValue *value = objc_getAssociatedObject(view, kSolunaSelectedRangeKey);
    if (value == nil) {
        return NSMakeRange(NSNotFound, 0);
    }
    return [value rangeValue];
}

static void
soluna_set_event_consumed(NSView *view, bool consumed) {
    if (consumed) {
        objc_setAssociatedObject(view, kSolunaConsumedFlagKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    } else {
        objc_setAssociatedObject(view, kSolunaConsumedFlagKey, nil, OBJC_ASSOCIATION_ASSIGN);
    }
}

static bool
soluna_event_consumed(NSView *view) {
    NSNumber *flag = objc_getAssociatedObject(view, kSolunaConsumedFlagKey);
    return flag && [flag boolValue];
}

static void
soluna_emit_nsstring(NSString *text) {
    if (text == nil || text.length == 0) {
        return;
    }
    uint32_t mods = soluna_modifiers_from_event([NSApp currentEvent]);
    [text enumerateSubstringsInRange:NSMakeRange(0, text.length)
        options:NSStringEnumerationByComposedCharacterSequences
        usingBlock:^(NSString * _Nullable substring, NSRange substringRange, NSRange enclosingRange, BOOL * _Nonnull stop) {
            (void)substringRange;
            (void)enclosingRange;
            (void)stop;
            uint32_t codepoint = soluna_utf32_from_substring(substring);
            if (codepoint != 0) {
                soluna_emit_char(codepoint, mods, false);
            }
        }];
}

static NSRect
soluna_current_caret_screen_rect(NSView *view) {
    NSRect caret = soluna_current_caret_local_rect(view);
    caret = [view convertRect:caret toView:nil];
    if (view.window) {
        caret = [view.window convertRectToScreen:caret];
    }
    return caret;
}

@interface _sapp_macos_view (SolunaIME) <NSTextInputClient>
- (void)soluna_keyDown:(NSEvent *)event;
@end

@implementation _sapp_macos_view (SolunaIME)

- (void)soluna_keyDown:(NSEvent *)event {
    soluna_set_event_consumed(self, false);
    BOOL handled = [[self inputContext] handleEvent:event];
    bool consumed = soluna_event_consumed(self);
    bool hasMarked = soluna_view_has_marked_text(self);
    bool swallow_now = handled && (consumed || hasMarked);
    if (swallow_now || g_soluna_macos_composition) {
        g_soluna_macos_composition = hasMarked;
        return;
    }
    g_soluna_macos_composition = false;
    [self soluna_keyDown:event];
    if (handled && (consumed || hasMarked)) {
        return;
    }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString *plain = soluna_plain_string(string);
    soluna_store_marked_text(self, nil, NSMakeRange(NSNotFound, 0));
    if (plain.length > 0) {
        soluna_emit_nsstring(plain);
        soluna_set_event_consumed(self, true);
    } else {
        soluna_set_event_consumed(self, false);
    }
    g_soluna_macos_composition = false;
    soluna_macos_hide_ime_label();
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString *plain = soluna_plain_string(string);
    soluna_store_marked_text(self, plain, selectedRange);
    g_soluna_macos_composition = (string != nil);
    soluna_macos_position_ime_label(self, plain, selectedRange);
}

- (void)unmarkText {
    soluna_store_marked_text(self, nil, NSMakeRange(NSNotFound, 0));
    g_soluna_macos_composition = false;
    soluna_macos_hide_ime_label();
}

- (NSRange)selectedRange {
    NSRange range = soluna_current_selected_range(self);
    if (range.location == NSNotFound) {
        return NSMakeRange(0, 0);
    }
    return range;
}

- (NSRange)markedRange {
    NSString *text = soluna_current_marked_text(self);
    if (text && text.length > 0) {
        return NSMakeRange(0, text.length);
    }
    return NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText {
    NSString *text = soluna_current_marked_text(self);
    return text && text.length > 0;
}

- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    NSString *text = soluna_current_marked_text(self);
    if (text == nil) {
        return nil;
    }
    if (range.location == NSNotFound) {
        return nil;
    }
    NSUInteger end = range.location + range.length;
    if (end > text.length) {
        return nil;
    }
    NSString *substr = [text substringWithRange:range];
    if (actualRange) {
        *actualRange = range;
    }
    return [[[NSAttributedString alloc] initWithString:substr] autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    (void)point;
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    if (actualRange) {
        *actualRange = range;
    }
    return soluna_current_caret_screen_rect(self);
}

- (void)doCommandBySelector:(SEL)selector {
    id nextResponder = [self nextResponder];
    if ([nextResponder respondsToSelector:@selector(doCommandBySelector:)]) {
        [nextResponder doCommandBySelector:selector];
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

@end

void
soluna_macos_install_ime(void) {
    static bool installed = false;
    if (installed) {
        return;
    }
    Class viewCls = NSClassFromString(@"_sapp_macos_view");
    if (!viewCls) {
        return;
    }
    Method original = class_getInstanceMethod(viewCls, @selector(keyDown:));
    Method replacement = class_getInstanceMethod(viewCls, @selector(soluna_keyDown:));
    if (original && replacement) {
        method_exchangeImplementations(original, replacement);
    }
    installed = true;
}

void
soluna_macos_apply_ime_rect(void) {
    if (g_soluna_ime_label && ![g_soluna_ime_label isHidden]) {
        NSView *view = [g_soluna_ime_label superview];
        if (view) {
            soluna_macos_refresh_ime_label(view);
        }
    }
}

bool
soluna_macos_is_composition_active(void) {
    return g_soluna_macos_composition;
}
