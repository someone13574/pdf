#include "pdf/fonts/encoding.h"

#include <stdint.h>
#include <string.h>

#include "../deserialize.h"
#include "logger/log.h"
#include "pdf/object.h"
#include "pdf/resolver.h"
#include "pdf_error/error.h"

static const char* pdf_encoding_common_lookup[256] = {
    [32] = "space",
    [33] = "exclam",
    [34] = "quotedbl",
    [35] = "numbersign",
    [36] = "dollar",
    [37] = "percent",
    [38] = "ampersand",
    [40] = "parenleft",
    [41] = "parenright",
    [42] = "asterisk",
    [43] = "plus",
    [44] = "comma",
    [45] = "hyphen",
    [46] = "period",
    [47] = "slash",
    [48] = "zero",
    [49] = "one",
    [50] = "two",
    [51] = "three",
    [52] = "four",
    [53] = "five",
    [54] = "six",
    [55] = "seven",
    [56] = "eight",
    [57] = "nine",
    [58] = "colon",
    [59] = "semicolon",
    [60] = "less",
    [61] = "equal",
    [62] = "greater",
    [63] = "question",
    [64] = "at",
    [65] = "A",
    [66] = "B",
    [67] = "C",
    [68] = "D",
    [69] = "E",
    [70] = "F",
    [71] = "G",
    [72] = "H",
    [73] = "I",
    [74] = "J",
    [75] = "K",
    [76] = "L",
    [77] = "M",
    [78] = "N",
    [79] = "O",
    [80] = "P",
    [81] = "Q",
    [82] = "R",
    [83] = "S",
    [84] = "T",
    [85] = "U",
    [86] = "V",
    [87] = "W",
    [88] = "X",
    [89] = "Y",
    [90] = "Z",
    [91] = "bracketleft",
    [92] = "backslash",
    [93] = "bracketright",
    [94] = "asciicircum",
    [95] = "underscore",
    [97] = "a",
    [98] = "b",
    [99] = "c",
    [100] = "d",
    [101] = "e",
    [102] = "f",
    [103] = "g",
    [104] = "h",
    [105] = "i",
    [106] = "j",
    [107] = "k",
    [108] = "l",
    [109] = "m",
    [110] = "n",
    [111] = "o",
    [112] = "p",
    [113] = "q",
    [114] = "r",
    [115] = "s",
    [116] = "t",
    [117] = "u",
    [118] = "v",
    [119] = "w",
    [120] = "x",
    [121] = "y",
    [122] = "z",
    [123] = "braceleft",
    [124] = "bar",
    [125] = "braceright",
    [126] = "asciitilde",
    [129] = "Aring",
    [141] = "ccedilla",
    [143] = "egrave",
    [144] = "ecircumflex",
    [157] = "ugrave",
    [160] = "dagger",
    [162] = "cent",
    [163] = "sterling",
    [173] = "guilsinglright",
    [176] = "degree",
    [181] = "mu",
    [182] = "paragraph",
    [215] = "multiply",
    [240] = "eth"
};

static const char* pdf_encoding_mac_roman_lookup[256] = {
    [39] = "quotesingle",
    [96] = "grave",
    [128] = "Adieresis",
    [130] = "Ccedilla",
    [131] = "Eacute",
    [132] = "Ntilde",
    [133] = "Odieresis",
    [134] = "Udieresis",
    [135] = "aacute",
    [136] = "agrave",
    [137] = "acircumflex",
    [138] = "adieresis",
    [139] = "atilde",
    [140] = "aring",
    [142] = "eacute",
    [145] = "edieresis",
    [146] = "iacute",
    [147] = "igrave",
    [148] = "icircumflex",
    [149] = "idieresis",
    [150] = "ntilde",
    [151] = "oacute",
    [152] = "ograve",
    [153] = "ocircumflex",
    [154] = "odieresis",
    [155] = "otilde",
    [156] = "uacute",
    [158] = "ucircumflex",
    [159] = "udieresis",
    [161] = "degree",
    [164] = "section",
    [165] = "bullet",
    [166] = "paragraph",
    [167] = "germandbls",
    [168] = "registered",
    [169] = "copyright",
    [170] = "trademark",
    [171] = "acute",
    [172] = "dieresis",
    [174] = "AE",
    [175] = "Oslash",
    [177] = "plusminus",
    [180] = "yen",
    [187] = "ordfeminine",
    [188] = "ordmasculine",
    [190] = "ae",
    [191] = "oslash",
    [192] = "questiondown",
    [193] = "exclamdown",
    [194] = "logicalnot",
    [196] = "florin",
    [199] = "guillemotleft",
    [200] = "guillemotright",
    [201] = "ellipsis",
    [203] = "Agrave",
    [204] = "Atilde",
    [205] = "Otilde",
    [206] = "OE",
    [207] = "oe",
    [208] = "endash",
    [209] = "emdash",
    [210] = "quotedblleft",
    [211] = "quotedblright",
    [212] = "quoteleft",
    [213] = "quoteright",
    [214] = "divide",
    [216] = "ydieresis",
    [217] = "Ydieresis",
    [218] = "fraction",
    [219] = "currency",
    [220] = "guilsinglleft",
    [221] = "guilsinglright",
    [222] = "fi",
    [223] = "fl",
    [224] = "daggerdbl",
    [225] = "periodcentered",
    [226] = "quotesinglbase",
    [227] = "quotedblbase",
    [228] = "perthousand",
    [229] = "Acircumflex",
    [230] = "Ecircumflex",
    [231] = "Aacute",
    [232] = "Edieresis",
    [233] = "Egrave",
    [234] = "Iacute",
    [235] = "Icircumflex",
    [236] = "Idieresis",
    [237] = "Igrave",
    [238] = "Oacute",
    [239] = "Ocircumflex",
    [241] = "Ograve",
    [242] = "Uacute",
    [243] = "Ucircumflex",
    [244] = "Ugrave",
    [245] = "dotlessi",
    [246] = "circumflex",
    [247] = "tilde",
    [248] = "macron",
    [249] = "breve",
    [250] = "dotaccent",
    [251] = "ring",
    [252] = "cedilla",
    [253] = "hungarumlaut",
    [254] = "ogonek",
    [255] = "caron"
};

const char* pdf_decode_mac_roman_codepoint(uint8_t codepoint) {
    const char* common = pdf_encoding_common_lookup[codepoint];
    if (common) {
        return common;
    }

    return pdf_encoding_mac_roman_lookup[codepoint];
}

static const char* pdf_encoding_win_ansi_lookup[256] = {
    [39] = "quotesingle",
    [96] = "grave",
    [128] = "Euro",
    [130] = "quotesinglbase",
    [131] = "florin",
    [132] = "quotedblbase",
    [133] = "ellipsis",
    [134] = "dagger",
    [135] = "daggerdbl",
    [136] = "circumflex",
    [137] = "perthousand",
    [138] = "Scaron",
    [139] = "guilsinglleft",
    [140] = "OE",
    [142] = "Zcaron",
    [145] = "quoteleft",
    [146] = "quoteright",
    [147] = "quotedblleft",
    [148] = "quotedblright",
    [149] = "bullet",
    [150] = "endash",
    [151] = "emdash",
    [152] = "tilde",
    [153] = "trademark",
    [154] = "scaron",
    [155] = "guilsinglright",
    [156] = "oe",
    [158] = "Zcaron",
    [159] = "Ydieresis",
    [161] = "exclamdown",
    [164] = "currency",
    [165] = "yen",
    [166] = "brokenbar",
    [167] = "section",
    [168] = "dieresis",
    [169] = "copyright",
    [170] = "ordfeminine",
    [171] = "guillemotleft",
    [172] = "logicalnot",
    [174] = "registered",
    [175] = "macron",
    [177] = "plusminus",
    [178] = "twosuperior",
    [179] = "threesuperior",
    [180] = "acute",
    [183] = "periodcentered",
    [184] = "cedilla",
    [185] = "onesuperior",
    [186] = "ordmasculine",
    [187] = "guillemotright",
    [188] = "onequarter",
    [189] = "onehalf",
    [190] = "threequarters",
    [191] = "questiondown",
    [192] = "Agrave",
    [193] = "Aacute",
    [194] = "Acircumflex",
    [195] = "Atilde",
    [196] = "Adieresis",
    [197] = "Aring",
    [198] = "AE",
    [199] = "Ccedilla",
    [200] = "Egrave",
    [201] = "Eacute",
    [202] = "Ecircumflex",
    [203] = "Edieresis",
    [204] = "Igrave",
    [205] = "Iacute",
    [206] = "Icircumflex",
    [207] = "Idieresis",
    [208] = "Eth",
    [209] = "Ntilde",
    [210] = "Ograve",
    [211] = "Oacute",
    [212] = "Ocircumflex",
    [213] = "Otilde",
    [214] = "Odieresis",
    [216] = "Oslash",
    [217] = "Ugrave",
    [218] = "Uacute",
    [219] = "Ucircumflex",
    [220] = "Udieresis",
    [221] = "Yacute",
    [222] = "Thorn",
    [223] = "germandbls",
    [224] = "agrave",
    [225] = "aacute",
    [226] = "acircumflex",
    [227] = "atilde",
    [228] = "adieresis",
    [229] = "aring",
    [230] = "ae",
    [231] = "ccedilla",
    [232] = "egrave",
    [233] = "eacute",
    [234] = "ecircumflex",
    [235] = "edieresis",
    [236] = "igrave",
    [237] = "iacute",
    [238] = "icircumflex",
    [239] = "idieresis",
    [241] = "ntilde",
    [242] = "ograve",
    [243] = "oacute",
    [244] = "ocircumflex",
    [245] = "otilde",
    [246] = "odieresis",
    [247] = "divide",
    [248] = "oslash",
    [249] = "ugrave",
    [250] = "uacute",
    [251] = "ucircumflex",
    [252] = "udieresis",
    [253] = "yacute",
    [254] = "thorn",
    [255] = "ydieresis"
};

const char* pdf_decode_win_ansi_codepoint(uint8_t codepoint) {
    const char* common = pdf_encoding_common_lookup[codepoint];
    if (common) {
        return common;
    }

    return pdf_encoding_win_ansi_lookup[codepoint];
}

static const char* pdf_encoding_adobe_standard_lookup[256] = {
    [39] = "quoteright",
    [96] = "quoteleft",
    [161] = "exclamdown",
    [164] = "fraction",
    [165] = "yen",
    [166] = "florin",
    [167] = "section",
    [168] = "currency",
    [169] = "quotesingle",
    [170] = "quotedblleft",
    [171] = "guillemotleft",
    [172] = "guilsinglleft",
    [174] = "fi",
    [175] = "fl",
    [177] = "endash",
    [178] = "dagger",
    [179] = "daggerdbl",
    [180] = "periodcentered",
    [183] = "bullet",
    [184] = "quotesinglbase",
    [185] = "quotedblbase",
    [186] = "quotedblright",
    [187] = "guillemotright",
    [188] = "ellipsis",
    [189] = "perthousand",
    [191] = "questiondown",
    [193] = "grave",
    [194] = "acute",
    [195] = "circumflex",
    [196] = "tilde",
    [197] = "macron",
    [198] = "breve",
    [199] = "dotaccent",
    [200] = "dieresis",
    [202] = "ring",
    [203] = "cedilla",
    [205] = "hungarumlaut",
    [206] = "ogonek",
    [207] = "caron",
    [208] = "emdash",
    [225] = "AE",
    [227] = "ordfeminine",
    [232] = "Lslash",
    [233] = "Oslash",
    [234] = "OE",
    [235] = "ordmasculine",
    [241] = "ae",
    [245] = "dotlessi",
    [248] = "lslash",
    [249] = "oslash",
    [250] = "oe",
    [251] = "germandbls"
};

const char* pdf_decode_adobe_standard_codepoint(uint8_t codepoint) {
    const char* common = pdf_encoding_common_lookup[codepoint];
    if (common) {
        return common;
    }

    return pdf_encoding_adobe_standard_lookup[codepoint];
}

PdfError* pdf_deserialize_encoding_dict(
    const PdfObject* object,
    PdfEncodingDict* target_ptr,
    PdfResolver* resolver
) {
    RELEASE_ASSERT(object);
    RELEASE_ASSERT(target_ptr);
    RELEASE_ASSERT(resolver);

    // Resolve
    PdfObject resolved;
    PDF_PROPAGATE(pdf_resolve_object(resolver, object, &resolved, true));

    // Attempt to deserialize name
    if (resolved.type == PDF_OBJECT_TYPE_NAME) {
        target_ptr->type = (PdfNameOptional) {.has_value = false};
        target_ptr->base_encoding =
            (PdfNameOptional) {.has_value = true, .value = resolved.data.name};
        target_ptr->differences = (PdfArrayOptional) {.has_value = false};
        return NULL;
    }

    // Deserialize dict
    PdfFieldDescriptor fields[] = {
        PDF_FIELD(
            "Type",
            &target_ptr->type,
            PDF_DESERDE_OPTIONAL(
                pdf_name_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "BaseEncoding",
            &target_ptr->base_encoding,
            PDF_DESERDE_OPTIONAL(
                pdf_name_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_NAME)
            )
        ),
        PDF_FIELD(
            "Differences",
            &target_ptr->differences,
            PDF_DESERDE_OPTIONAL(
                pdf_array_op_init,
                PDF_DESERDE_OBJECT(PDF_OBJECT_TYPE_ARRAY)
            )
        )
    };

    PDF_PROPAGATE(pdf_deserialize_dict(
        object,
        fields,
        sizeof(fields) / sizeof(PdfFieldDescriptor),
        false,
        resolver,
        "PdfEncodingDict"
    ));

    if (target_ptr->type.has_value
        && strcmp(target_ptr->type.value, "Encoding") != 0) {
        return PDF_ERROR(PDF_ERR_INCORRECT_TYPE, "`Type` must be `Encoding`");
    }

    if (target_ptr->base_encoding.has_value) {
        if (strcmp(target_ptr->base_encoding.value, "MacRomanEncoding") != 0
            && strcmp(target_ptr->base_encoding.value, "MacExpertEncoding") != 0
            && strcmp(target_ptr->base_encoding.value, "WinAnsiEncoding")
                   != 0) {
            return PDF_ERROR(
                PDF_ERR_INVALID_OBJECT,
                "Invalid `BaseEncoding` value `%s`",
                target_ptr->base_encoding.value
            );
        }
    }

    RELEASE_ASSERT(!target_ptr->differences.has_value, "Unimplemented");

    return NULL;
}

const char* pdf_encoding_map_codepoint(
    const PdfEncodingDict* encoding_dict,
    uint8_t codepoint
) {
    RELEASE_ASSERT(encoding_dict);

    const char* base = NULL;
    if (!encoding_dict->base_encoding.has_value) {
        base = pdf_decode_adobe_standard_codepoint(codepoint);
    } else if (strcmp(encoding_dict->base_encoding.value, "MacRomanEncoding")
               == 0) {
        base = pdf_decode_mac_roman_codepoint(codepoint);
    } else if (strcmp(encoding_dict->base_encoding.value, "MacExpertEncoding")
               == 0) {
        LOG_TODO("MacExpertEncoding");
    } else if (strcmp(encoding_dict->base_encoding.value, "WinAnsiEncoding")
               == 0) {
        base = pdf_decode_win_ansi_codepoint(codepoint);
    }

    RELEASE_ASSERT(base);
    if (encoding_dict->differences.has_value) {
        LOG_WARN(FONT, "Encoding has differences array! TODO");
    }
    // TODO: differences array
    return base;
}

DESERDE_IMPL_TRAMPOLINE(
    pdf_deserialize_encoding_dict_trampoline,
    pdf_deserialize_encoding_dict
)

DESERDE_IMPL_OPTIONAL(PdfEncodingDictOptional, pdf_encoding_dict_op_init)
