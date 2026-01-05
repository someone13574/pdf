# PDF

This is a parser/renderer for PDF files, written from scratch using only the C standard library. It can render simple files, and makes heavy use of panics for anything unimplemented. It should compile on gcc and clang, and maybe msvc (though msvc is rarely tested, so no guarantees).

So far the following have been implemented:

- An arena allocator and arena-backed type-safe vectors, arrays, strings, and linked-lists
- A logging library, error library, and testing library
  - The testing library supports in-source tests by (ab)using a custom linker section as a test registry. The testing library only works on clang/gcc.
- A parser for all core pdf object types
- A deserialization framework for converting pdf objects into C structures
  - This supports optional fields, lazy and eagar indirect references, dynamic arrays, fixed arrays, and custom deserialization functions. It makes heavy use of macros.
- A zlib/deflate decompressor for filtered streams
- A sfnt (ttf) file parser/renderer
- A CFF font parser/renderer
- Support for CID fonts
- A minimal postscript interpreter for parsing cmap files and running pdf functions
  - This only implements the operators needed for these tasks
- A simple (incomplete) renderer
- A SVG canvas

Future expansions:

- Type1 font parser
- Support for patterns and shadings
- Image support
- ICC parser
- Raster canvas backend

# Resources

- [PDF Specification](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf)
  - [Let's write a PDF file](https://speakerdeck.com/ange/lets-write-a-pdf-file)
- [PostScript Language Reference](https://www.adobe.com/jp/print/postscript/pdfs/PLRM.pdf)
- [Adobe CMap and CIDFont Files Specification](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5014.CIDFont_Spec.pdf)
- [The Compact Font Format Specification](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf)
  - [The Type 2 Charstring Format](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf)
- [TrueType Reference Manual](https://developer.apple.com/fonts/TrueType-Reference-Manual/)
- [BMP Wiki](https://en.wikipedia.org/wiki/BMP_file_format)
- [Partitioning a Polygon into _y_-monotone Pieces](https://www.youtube.com/watch?v=IkA-2Y9lBvM)
- [DEFLATE Compressed Data Format Specification](https://datatracker.ietf.org/doc/html/rfc1951)
  - [ZLIB Compressed Data Format Specification](https://datatracker.ietf.org/doc/html/rfc1950)
- [ICC Specification](https://www.color.org/specification/ICC.1-2022-05.pdf)
