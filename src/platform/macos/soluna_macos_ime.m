#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <stdint.h>

#include "ime_state.h"
#include "soluna_macos_ime.h"
#include "sokol/sokol_app.h"

@interface _sapp_macos_view : NSView
@end

static void soluna_emit_nsstring(NSString *text);

@interface SolunaIMETextView : NSTextView
@end

@implementation SolunaIMETextView
- (BOOL)isOpaque {
    return NO;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setEditable:NO];
        [self setSelectable:NO];
        [self setRichText:NO];
        [self setImportsGraphics:NO];
        [self setAutomaticQuoteSubstitutionEnabled:NO];
        [self setAutomaticDataDetectionEnabled:NO];
        [self setAutomaticSpellingCorrectionEnabled:NO];
        [self setDrawsBackground:NO];
        [self setHorizontallyResizable:YES];
        [self setVerticallyResizable:NO];
        [self setTextContainerInset:NSMakeSize(0, 0)];
        NSTextContainer *container = [self textContainer];
        if (container) {
            [container setLineFragmentPadding:0.0f];
            [container setWidthTracksTextView:NO];
            [container setContainerSize:NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX)];
        }
        [self setHidden:YES];
    }
    return self;
}

- (void)doCommandBySelector:(SEL)selector {
    (void)selector;
}
@end

// Forward declarations implemented in entry.c
void soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat);

static bool g_soluna_macos_composition = false;
static SolunaIMETextView *g_soluna_ime_label = nil;
static NSString *g_soluna_macos_ime_font_name = nil;
static CGFloat g_soluna_macos_ime_font_size = 14.0f;
static SolunaIMETextView *soluna_macos_ensure_ime_label(NSView *view);
static const void *const kSolunaMarkedTextKey = &kSolunaMarkedTextKey;
static const void *const kSolunaSelectedRangeKey = &kSolunaSelectedRangeKey;
static const void *const kSolunaConsumedFlagKey = &kSolunaConsumedFlagKey;

static NSFont *
soluna_macos_current_ime_font(void) {
    CGFloat size = g_soluna_macos_ime_font_size > 0.0f ? g_soluna_macos_ime_font_size : 14.0f;
    NSFont *font = nil;
    if (g_soluna_macos_ime_font_name) {
        font = [NSFont fontWithName:g_soluna_macos_ime_font_name size:size];
    }
    if (font == nil) {
        font = [NSFont systemFontOfSize:size];
    }
    return font;
}

static void
soluna_macos_apply_ime_font(void) {
    if (g_soluna_ime_label) {
        NSFont *font = soluna_macos_current_ime_font();
        if (font) {
            [g_soluna_ime_label setFont:font];
        }
    }
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

static void
soluna_macos_position_ime_input_view(NSView *view) {
    SolunaIMETextView *imeView = soluna_macos_ensure_ime_label(view);
    if (!imeView || !g_soluna_ime_rect.valid) {
        return;
    }
    NSRect caret = soluna_current_caret_local_rect(view);
    NSRect bounds = view.bounds;
    NSFont *font = soluna_macos_current_ime_font();
    CGFloat ascender = 0.0f;
    CGFloat descender = 0.0f;
    CGFloat leading = 0.0f;
    if (font) {
        ascender = MAX(font.ascender, 0.0f);
        descender = MIN(font.descender, 0.0f);
        leading = MAX(font.leading, 0.0f);
    }
    CGFloat lineHeight = ascender - descender + leading;
    if (lineHeight <= 0.0f) {
        lineHeight = g_soluna_macos_ime_font_size > 0.0f ? g_soluna_macos_ime_font_size : 14.0f;
    }
    lineHeight = MAX(lineHeight, MAX(caret.size.height, 1.0f));
    CGFloat baselineY = caret.origin.y + MAX(caret.size.height, 1.0f) * 0.5f;
    CGFloat frameX = caret.origin.x;
    CGFloat frameY = baselineY - ascender;
    CGFloat frameW = MAX(1.0f, NSMaxX(bounds) - frameX);
    CGFloat frameH = lineHeight;
    NSRect frame = NSMakeRect(frameX, frameY, frameW, frameH);
    CGFloat maxOriginX = NSMaxX(bounds) - 1.0f;
    if (maxOriginX < bounds.origin.x) {
        maxOriginX = bounds.origin.x;
    }
    if (frame.origin.x < bounds.origin.x) frame.origin.x = bounds.origin.x;
    if (frame.origin.x > maxOriginX) frame.origin.x = maxOriginX;
    if (frame.origin.y < bounds.origin.y) frame.origin.y = bounds.origin.y;
    if (NSMaxX(frame) > NSMaxX(bounds)) frame.size.width = MAX(1.0f, NSMaxX(bounds) - frame.origin.x);
    if (NSMaxY(frame) > NSMaxY(bounds)) frame.origin.y = NSMaxY(bounds) - frame.size.height;
    [imeView setFrame:NSIntegralRect(frame)];
    [imeView setHidden:NO];
}

static SolunaIMETextView *
soluna_macos_ensure_ime_label(NSView *view) {
    if (g_soluna_ime_label == nil) {
        g_soluna_ime_label = [[SolunaIMETextView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0)];
        [g_soluna_ime_label setHidden:YES];
        [g_soluna_ime_label setTranslatesAutoresizingMaskIntoConstraints:YES];
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
        [g_soluna_ime_label setString:@""];
        [g_soluna_ime_label setHidden:YES];
    }
}

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
    if (text != nil && text.length > 0) {
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
    return text != nil && text.length > 0;
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
soluna_update_ime_label(NSView *view, id markedText, NSRange selectedRange) {
    NSString *plain = soluna_plain_string(markedText);
    if (!g_soluna_ime_rect.valid || plain.length == 0) {
        soluna_macos_hide_ime_label();
        return;
    }
    SolunaIMETextView *imeView = soluna_macos_ensure_ime_label(view);
    if (!imeView) {
        return;
    }
    soluna_macos_position_ime_input_view(view);
    NSMutableAttributedString *attr = nil;
    if ([markedText isKindOfClass:[NSAttributedString class]]) {
        attr = [[(NSAttributedString *)markedText mutableCopy] autorelease];
    } else {
        attr = [[[NSMutableAttributedString alloc] initWithString:plain] autorelease];
    }
    if (attr.length > 0) {
        NSRange full = NSMakeRange(0, attr.length);
        NSFont *font = soluna_macos_current_ime_font();
        if (font) {
            [attr addAttribute:NSFontAttributeName value:font range:full];
        }
        [attr addAttribute:NSForegroundColorAttributeName value:[NSColor textColor] range:full];
        if ([attr attribute:NSUnderlineStyleAttributeName atIndex:0 effectiveRange:NULL] == nil) {
            [attr addAttribute:NSUnderlineStyleAttributeName value:@(NSUnderlineStyleSingle) range:full];
        }
    }
    [[imeView textStorage] setAttributedString:attr];
    if (selectedRange.location != NSNotFound && NSMaxRange(selectedRange) <= attr.length) {
        [imeView setSelectedRange:selectedRange];
    } else {
        [imeView setSelectedRange:NSMakeRange(attr.length, 0)];
    }
    [imeView setHidden:NO];
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
    bool wasComposing = g_soluna_macos_composition || soluna_view_has_marked_text(self);
    if (g_soluna_ime_rect.valid && wasComposing) {
        soluna_macos_position_ime_input_view(self);
    }
    soluna_set_event_consumed(self, false);
    BOOL handled = [[self inputContext] handleEvent:event];
    bool consumed = soluna_event_consumed(self);
    bool hasMarked = soluna_view_has_marked_text(self);
    if (handled && (consumed || hasMarked)) {
        g_soluna_macos_composition = true;
        return;
    }
    if (g_soluna_macos_composition && hasMarked) {
        return;
    }
    g_soluna_macos_composition = false;
    [self soluna_keyDown:event];
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
    g_soluna_macos_composition = true;
    soluna_update_ime_label(self, string, selectedRange);
    soluna_set_event_consumed(self, true);
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
    if (text != nil && text.length > 0) {
        return NSMakeRange(0, text.length);
    }
    return NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText {
    return soluna_view_has_marked_text(self);
}

- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    NSString *text = soluna_current_marked_text(self);
    if (text == nil || range.location == NSNotFound) {
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
    if (!g_soluna_ime_rect.valid) {
        soluna_macos_hide_ime_label();
        return;
    }
    if (g_soluna_ime_label) {
        NSView *view = [g_soluna_ime_label superview];
        if (view) {
            soluna_macos_position_ime_input_view(view);
        }
    }
}

bool
soluna_macos_is_composition_active(void) {
    return g_soluna_macos_composition;
}
