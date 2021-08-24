#include "leanify.h"

#include <algorithm>
#include <iostream>

#include <zopfli/zlib_container.h>
#include <zopflipng/lodepng/lodepng.h>

#include "formats/data_uri.h"
#include "formats/format.h"
#include "formats/ico.h"
#include "formats/jpeg.h"
#include "formats/mime.h"
#include "formats/png.h"
#include "formats/xml.h"
#include "utils.h"

using std::cerr;
using std::endl;
using std::string;

Format* GetType(void* file_pointer, size_t file_size, const string& filename) {
  if (depth > max_depth)
    return new Format(file_pointer, file_size);

  if (!filename.empty()) {
    size_t dot = filename.find_last_of('.');
    if (dot != string::npos) {
      string ext = filename.substr(dot + 1);
      // toupper
      for (auto& c : ext)
        c &= ~0x20;

      if (ext == "HTML" || ext == "HTM" || ext == "JS" || ext == "CSS") {
        VerbosePrint(ext, " detected.");
        return new DataURI(file_pointer, file_size);
      }
      if (ext == "MHT" || ext == "MHTML" || ext == "MIM" || ext == "MIME" || ext == "EML") {
        VerbosePrint(ext, " detected.");
        return new Mime(file_pointer, file_size);
      }
    }
  }
  if (memcmp(file_pointer, Png::header_magic, sizeof(Png::header_magic)) == 0) {
    VerbosePrint("PNG detected.");
    return new Png(file_pointer, file_size);
  } else if (memcmp(file_pointer, Jpeg::header_magic, sizeof(Jpeg::header_magic)) == 0) {
    VerbosePrint("JPEG detected.");
    return new Jpeg(file_pointer, file_size);
  } else if (memcmp(file_pointer, Ico::header_magic, sizeof(Ico::header_magic)) == 0) {
    VerbosePrint("ICO detected.");
    return new Ico(file_pointer, file_size);
  } else {
    // XML file does not have header magic
    // have to parse and see if there are any errors.
    {
      Xml* x = new Xml(file_pointer, file_size);
      if (x->IsValid()) {
        VerbosePrint("XML detected.");
        return x;
      }
      delete x;
    }
  }

  VerbosePrint("Format not supported!");
  // for unsupported format, just memmove it.
  return new Format(file_pointer, file_size);
}

// Leanify the file
// and move the file ahead size_leanified bytes
// the new location of the file will be file_pointer - size_leanified
// it's designed this way to avoid extra memmove or memcpy
// return new size
size_t LeanifyFile(void* file_pointer, size_t file_size, size_t size_leanified /*= 0*/,
                   const string& filename /*= ""*/) {
  Format* f = GetType(file_pointer, file_size, filename);
  size_t r = f->Leanify(size_leanified);
  delete f;
  return r;
}

size_t ZlibRecompress(uint8_t* src, size_t src_len, size_t size_leanified /*= 0*/) {
  if (!is_fast) {
    size_t uncompressed_size = 0;
    uint8_t* buffer = nullptr;
    if (lodepng_zlib_decompress(&buffer, &uncompressed_size, src, src_len, &lodepng_default_decompress_settings) ||
        !buffer) {
      cerr << "Decompress Zlib data failed." << endl;
    } else {
      ZopfliOptions zopfli_options;
      ZopfliInitOptions(&zopfli_options);
      zopfli_options.numiterations = iterations;

      size_t new_size = 0;
      uint8_t* out_buffer = nullptr;
      ZopfliZlibCompress(&zopfli_options, buffer, uncompressed_size, &out_buffer, &new_size);
      free(buffer);
      if (new_size < src_len) {
        memcpy(src - size_leanified, out_buffer, new_size);
        free(out_buffer);
        return new_size;
      }
      free(out_buffer);
    }
  }

  memmove(src - size_leanified, src, src_len);
  return src_len;
}