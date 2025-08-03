#include "fmt.h"
#include <lib/str.h>

#include <physical_alloc.h>
#include <stdarg.h>
#include <stdbool.h>

#include <device/console.h>

enum format_type {
  FORMAT_TYPE_INT,
  FORMAT_TYPE_UINT,
  FORMAT_TYPE_HEX,
  FORMAT_TYPE_CHAR,
  FORMAT_TYPE_STR,
  FORMAT_TYPE_PTR,
  FORMAT_TYPE_BINARY,
  FORMAT_TYPE_INVALID,
};

#define FORMAT_TYPE_INT_STR "int"
#define FORMAT_TYPE_UINT_STR "uint"
#define FORMAT_TYPE_HEX_STR "hex"
#define FORMAT_TYPE_CHAR_STR "char"
#define FORMAT_TYPE_STR_STR "str"
#define FORMAT_TYPE_PTR_STR "ptr"
#define FORMAT_TYPE_BINARY_STR "binary"

enum format_type format_type_from_str(const char *str);
const char *format_type_to_str(enum format_type type);

static inline uint64_t get_uint_arg(va_list *ap) {
  return va_arg(*ap, uint64_t);
}
static inline int64_t get_int_arg(va_list *ap) { return va_arg(*ap, int64_t); }
static inline char get_char_arg(va_list *ap) { return (char)va_arg(*ap, int); }
static inline char *get_str_arg(va_list *ap) { return va_arg(*ap, char *); }
static inline void *get_ptr_arg(va_list *ap) { return va_arg(*ap, void *); }

enum format_case {
  FORMAT_CASE_LOWER,
  FORMAT_CASE_UPPER,
  FORMAT_CASE_INVALID,
};

#define FORMAT_CASE_LOWER_STR "lower"
#define FORMAT_CASE_UPPER_STR "upper"

enum format_case format_case_from_str(const char *str);
const char *format_case_to_str(enum format_case format_case);

enum format_justify {
  FORMAT_JUSTIFY_LEFT,
  FORMAT_JUSTIFY_RIGHT,
  FORMAT_JUSTIFY_INVALID,
};

#define FORMAT_JUSTIFY_LEFT_STR "left"
#define FORMAT_JUSTIFY_RIGHT_STR "right"

enum format_justify format_justify_from_str(const char *str);
const char *format_justify_to_str(enum format_justify format_justify);

enum format_sign {
  FORMAT_SIGN_AUTO,
  FORMAT_SIGN_FORCE,
  FORMAT_SIGN_SPACE,
  FORMAT_SIGN_INVALID,
};

#define FORMAT_SIGN_AUTO_STR "auto"
#define FORMAT_SIGN_FORCE_STR "force"
#define FORMAT_SIGN_SPACE_STR "space"

enum format_sign format_sign_from_str(const char *str);
const char *format_sign_to_str(enum format_sign format_sign);

enum format_prefix {
  FORMAT_PREFIX_NONE,
  FORMAT_PREFIX_AUTO,
  FORMAT_PREFIX_INVALID,
};

#define FORMAT_PREFIX_NONE_STR "none"
#define FORMAT_PREFIX_AUTO_STR "auto"

enum format_prefix format_prefix_from_str(const char *str);
const char *format_prefix_to_str(enum format_prefix format_prefix);

enum format_decimal_point {
  FORMAT_DECIMAL_POINT_AUTO,
  FORMAT_DECIMAL_POINT_FORCE,
  FORMAT_DECIMAL_POINT_INVALID,
};

#define FORMAT_DECIMAL_POINT_AUTO_STR "auto"
#define FORMAT_DECIMAL_POINT_FORCE_STR "force"

enum format_decimal_point format_decimal_point_from_str(const char *str);
const char *
format_decimal_point_to_str(enum format_decimal_point format_decimal_point);

enum format_left_pad {
  FORMAT_LEFT_PAD_SPACE,
  FORMAT_LEFT_PAD_ZERO,
  FORMAT_LEFT_PAD_INVALID,
};

#define FORMAT_LEFT_PAD_SPACE_STR "space"
#define FORMAT_LEFT_PAD_ZERO_STR "zero"

enum format_left_pad format_left_pad_from_str(const char *str);
const char *format_left_pad_to_str(enum format_left_pad format_left_pad);

struct format {
  enum format_type format_type;
  enum format_case format_case;
  enum format_justify format_justify;
  enum format_sign format_sign;
  enum format_prefix format_prefix;
  enum format_decimal_point format_decimal_point;
  enum format_left_pad format_left_pad;

  bool width_from_args;
  int format_width;

  bool precision_from_args;
  int format_precision;
};

#define FORMAT_DEFAULT_TYPE FORMAT_TYPE_INVALID
#define FORMAT_DEFAULT_CASE FORMAT_CASE_LOWER
#define FORMAT_DEFAULT_JUSTIFY FORMAT_JUSTIFY_RIGHT
#define FORMAT_DEFAULT_SIGN FORMAT_SIGN_AUTO
#define FORMAT_DEFAULT_PREFIX FORMAT_PREFIX_NONE
#define FORMAT_DEFAULT_DECIMAL_POINT FORMAT_DECIMAL_POINT_AUTO
#define FORMAT_DEFAULT_LEFT_PAD FORMAT_LEFT_PAD_SPACE
#define FORMAT_DEFAULT_WIDTH_FROM_ARGS false
#define FORMAT_DEFAULT_WIDTH 0
#define FORMAT_DEFAULT_PRECISION_FROM_ARGS false
#define FORMAT_DEFAULT_PRECISION 0

#define FORMAT_KEY_TYPE "type"
#define FORMAT_KEY_CASE "case"
#define FORMAT_KEY_JUSTIFY "justify"
#define FORMAT_KEY_SIGN "sign"
#define FORMAT_KEY_PREFIX "prefix"
#define FORMAT_KEY_DECIMAL_POINT "decimal_point"
#define FORMAT_KEY_LEFT_PAD "left_pad"
#define FORMAT_KEY_WIDTH "width"
#define FORMAT_KEY_PRECISION "precision"

#define FORMAT_VALUE_SPECIAL "*"

enum format_type format_type_from_str(const char *str) {
  if (strcmp(str, FORMAT_TYPE_INT_STR))
    return FORMAT_TYPE_INT;
  else if (strcmp(str, FORMAT_TYPE_UINT_STR))
    return FORMAT_TYPE_UINT;
  else if (strcmp(str, FORMAT_TYPE_HEX_STR))
    return FORMAT_TYPE_HEX;
  else if (strcmp(str, FORMAT_TYPE_CHAR_STR))
    return FORMAT_TYPE_CHAR;
  else if (strcmp(str, FORMAT_TYPE_STR_STR))
    return FORMAT_TYPE_STR;
  else if (strcmp(str, FORMAT_TYPE_PTR_STR))
    return FORMAT_TYPE_PTR;
  else if (strcmp(str, FORMAT_TYPE_BINARY_STR))
    return FORMAT_TYPE_BINARY;
  return FORMAT_TYPE_INVALID;
}

const char *format_type_to_str(enum format_type type) {
  switch (type) {
  case FORMAT_TYPE_INT:
    return FORMAT_TYPE_INT_STR;
  case FORMAT_TYPE_UINT:
    return FORMAT_TYPE_UINT_STR;
  case FORMAT_TYPE_HEX:
    return FORMAT_TYPE_HEX_STR;
  case FORMAT_TYPE_CHAR:
    return FORMAT_TYPE_CHAR_STR;
  case FORMAT_TYPE_STR:
    return FORMAT_TYPE_STR_STR;
  case FORMAT_TYPE_PTR:
    return FORMAT_TYPE_PTR_STR;
  case FORMAT_TYPE_BINARY:
    return FORMAT_TYPE_BINARY_STR;
  default:
    return "INVALID_FORMAT_TYPE";
  }
}

enum format_case format_case_from_str(const char *str) {
  if (strcmp(str, FORMAT_CASE_LOWER_STR))
    return FORMAT_CASE_LOWER;
  else if (strcmp(str, FORMAT_CASE_UPPER_STR))
    return FORMAT_CASE_UPPER;
  return FORMAT_CASE_INVALID;
}

const char *format_case_to_str(enum format_case format_case) {
  switch (format_case) {
  case FORMAT_CASE_LOWER:
    return FORMAT_CASE_LOWER_STR;
  case FORMAT_CASE_UPPER:
    return FORMAT_CASE_UPPER_STR;
  default:
    return "INVALID_FORMAT_CASE";
  }
}

enum format_justify format_justify_from_str(const char *str) {
  if (strcmp(str, FORMAT_JUSTIFY_LEFT_STR))
    return FORMAT_JUSTIFY_LEFT;
  else if (strcmp(str, FORMAT_JUSTIFY_RIGHT_STR))
    return FORMAT_JUSTIFY_RIGHT;
  return FORMAT_JUSTIFY_INVALID;
}

const char *format_justify_to_str(enum format_justify format_justify) {
  switch (format_justify) {
  case FORMAT_JUSTIFY_LEFT:
    return FORMAT_JUSTIFY_LEFT_STR;
  case FORMAT_JUSTIFY_RIGHT:
    return FORMAT_JUSTIFY_RIGHT_STR;
  default:
    return "INVALID_FORMAT_JUSTIFY";
  }
}

enum format_sign format_sign_from_str(const char *str) {
  if (strcmp(str, FORMAT_SIGN_AUTO_STR))
    return FORMAT_SIGN_AUTO;
  else if (strcmp(str, FORMAT_SIGN_FORCE_STR))
    return FORMAT_SIGN_FORCE;
  else if (strcmp(str, FORMAT_SIGN_SPACE_STR))
    return FORMAT_SIGN_SPACE;
  return FORMAT_SIGN_INVALID;
}

const char *format_sign_to_str(enum format_sign format_sign) {
  switch (format_sign) {
  case FORMAT_SIGN_AUTO:
    return FORMAT_SIGN_AUTO_STR;
  case FORMAT_SIGN_FORCE:
    return FORMAT_SIGN_FORCE_STR;
  case FORMAT_SIGN_SPACE:
    return FORMAT_SIGN_SPACE_STR;
  default:
    return "INVALID_FORMAT_SIGN";
  }
}

enum format_prefix format_prefix_from_str(const char *str) {
  if (strcmp(str, FORMAT_PREFIX_AUTO_STR))
    return FORMAT_PREFIX_AUTO;
  else if (strcmp(str, FORMAT_PREFIX_NONE_STR))
    return FORMAT_PREFIX_NONE;
  return FORMAT_PREFIX_INVALID;
}

const char *format_prefix_to_str(enum format_prefix format_prefix) {
  switch (format_prefix) {
  case FORMAT_PREFIX_AUTO:
    return FORMAT_PREFIX_AUTO_STR;
  case FORMAT_PREFIX_NONE:
    return FORMAT_PREFIX_NONE_STR;
  default:
    return "INVALID_FORMAT_PREFIX";
  }
}

enum format_decimal_point format_decimal_point_from_str(const char *str) {
  if (strcmp(str, FORMAT_DECIMAL_POINT_AUTO_STR))
    return FORMAT_DECIMAL_POINT_AUTO;
  else if (strcmp(str, FORMAT_DECIMAL_POINT_FORCE_STR))
    return FORMAT_DECIMAL_POINT_FORCE;
  return FORMAT_DECIMAL_POINT_INVALID;
}

const char *
format_decimal_point_to_str(enum format_decimal_point format_decimal_point) {
  switch (format_decimal_point) {
  case FORMAT_DECIMAL_POINT_AUTO:
    return FORMAT_DECIMAL_POINT_AUTO_STR;
  case FORMAT_DECIMAL_POINT_FORCE:
    return FORMAT_DECIMAL_POINT_FORCE_STR;
  default:
    return "INVALID_FORMAT_DECIMAL_POINT";
  }
}

enum format_left_pad format_left_pad_from_str(const char *str) {
  if (strcmp(str, FORMAT_LEFT_PAD_SPACE_STR))
    return FORMAT_LEFT_PAD_SPACE;
  else if (strcmp(str, FORMAT_LEFT_PAD_ZERO_STR))
    return FORMAT_LEFT_PAD_ZERO;
  return FORMAT_LEFT_PAD_INVALID;
}

const char *format_left_pad_to_str(enum format_left_pad format_left_pad) {
  switch (format_left_pad) {
  case FORMAT_LEFT_PAD_SPACE:
    return FORMAT_LEFT_PAD_SPACE_STR;
  case FORMAT_LEFT_PAD_ZERO:
    return FORMAT_LEFT_PAD_ZERO_STR;
  default:
    return "INVALID_FORMAT_LEFT_PAD";
  }
}

char *format_dump(struct format *format, char *buf, size_t buf_len) {
  buf[0] = '\0';
  strcat(buf, "format {\n");
  strcat(buf, "   type: '");
  strcat(buf, format_type_to_str(format->format_type));
  strcat(buf, "',\n   case: '");
  strcat(buf, format_case_to_str(format->format_case));
  strcat(buf, "',\n   justify: '");
  strcat(buf, format_justify_to_str(format->format_justify));
  strcat(buf, "',\n   sign: '");
  strcat(buf, format_sign_to_str(format->format_sign));
  strcat(buf, "',\n   prefix: '");
  strcat(buf, format_prefix_to_str(format->format_prefix));
  strcat(buf, "',\n   decimal_point: '");
  strcat(buf, format_decimal_point_to_str(format->format_decimal_point));
  strcat(buf, "',\n   left_pad: '");
  strcat(buf, format_left_pad_to_str(format->format_left_pad));
  strcat(buf, "'\n}");
  return buf;
}

void format_parse_single(struct format *format, const char *key,
                         const char *val) {
  if (strcmp(key, FORMAT_KEY_TYPE)) {
    format->format_type = format_type_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_CASE)) {
    format->format_case = format_case_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_JUSTIFY)) {
    format->format_justify = format_justify_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_SIGN)) {
    format->format_sign = format_sign_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_PREFIX)) {
    format->format_prefix = format_prefix_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_DECIMAL_POINT)) {
    format->format_decimal_point = format_decimal_point_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_LEFT_PAD)) {
    format->format_left_pad = format_left_pad_from_str(val);
  } else if (strcmp(key, FORMAT_KEY_WIDTH)) {
    if (strcmp(val, FORMAT_VALUE_SPECIAL)) {
      format->width_from_args = true;
    } else {
      format->format_width = uintfstr(key);
    }
  } else if (strcmp(key, FORMAT_KEY_PRECISION)) {
    if (strcmp(val, FORMAT_VALUE_SPECIAL)) {
      format->precision_from_args = true;
    } else {
      format->format_precision = uintfstr(key);
    }
  }
}

void set_defaults(struct format *format) {
  format->format_type = FORMAT_DEFAULT_TYPE;
  format->format_case = FORMAT_DEFAULT_CASE;
  format->format_justify = FORMAT_DEFAULT_JUSTIFY;
  format->format_sign = FORMAT_DEFAULT_SIGN;
  format->format_prefix = FORMAT_DEFAULT_PREFIX;
  format->format_decimal_point = FORMAT_DEFAULT_DECIMAL_POINT;
  format->format_left_pad = FORMAT_DEFAULT_LEFT_PAD;
  format->width_from_args = FORMAT_DEFAULT_WIDTH_FROM_ARGS;
  format->format_width = FORMAT_DEFAULT_WIDTH;
  format->precision_from_args = FORMAT_DEFAULT_PRECISION_FROM_ARGS;
  format->format_precision = FORMAT_DEFAULT_PRECISION;
}

void format_parse(struct format *format, const char *str) {
  char key[32];
  char val[32];
  int key_i = 0;
  int val_i = 0;
  bool key_done = false;
  bool val_done = false;

  set_defaults(format);

  for (int i = 0; i < (int)strlen(str); i++) {
    if (str[i] == ':') {
      key_done = true;
      key[key_i] = '\0';
    } else if (str[i] == ',') {
      val_done = true;
      val[val_i] = '\0';
    } else if (str[i] == ' ') {
      continue;
    } else {
      if (!key_done) {
        key[key_i++] = str[i];
      } else if (!val_done) {
        val[val_i++] = str[i];
      }
    }

    // If both key and val are done, parse the pair
    if (key_done && val_done) {
#ifdef DEBUG
      term_puts("key: ");
      term_puts(key);
      term_puts(", val: ");
      term_puts(val);
      term_puts("\n");
#endif

      format_parse_single(format, key, val);

      // Reset for next pair
      key_i = 0;
      val_i = 0;
      key_done = false;
      val_done = false;
    }
  }

  // If we reached the end and there's a key-value pair that never hit a comma
  // (key_done true, but val_done false), parse it here.
  if (key_done && (val_i > 0)) {
    val[val_i] = '\0';

#ifdef DEBUG
    term_puts("key: ");
    term_puts(key);
    term_puts(", val: ");
    term_puts(val);
    term_puts("\n");
#endif

    format_parse_single(format, key, val);
  }
}

/* Format an int64_t into buf */
char *format_int(struct format *format, char *buf, int64_t val) {
  buf[0] = '\0';

  // Sign handling
  if (format->format_sign == FORMAT_SIGN_FORCE) {
    if (val < 0)
      strcat(buf, "-");
    else
      strcat(buf, "+");
  } else if (format->format_sign == FORMAT_SIGN_SPACE) {
    if (val < 0)
      strcat(buf, " ");
    else
      strcat(buf, "+");
  }

  // Convert number to string
  char num_buf[64];
  strfint(val, num_buf); // user-provided
  strcat(buf, num_buf);

  return buf;
}

/* Format a uint64_t into buf */
char *format_uint(struct format *format, char *buf, uint64_t val) {
  buf[0] = '\0';

  // Optional sign
  if (format->format_sign == FORMAT_SIGN_FORCE) {
    strcat(buf, "+");
  } else if (format->format_sign == FORMAT_SIGN_SPACE) {
    strcat(buf, " ");
  }

  // Convert number to string
  char num_buf[64];
  strfuint(val, num_buf); // user-provided
  strcat(buf, num_buf);

  return buf;
}

/* Format a hex */
char *format_hex(struct format *format, char *buf, uint64_t val) {
  buf[0] = '\0';

  // Optional sign
  if (format->format_sign == FORMAT_SIGN_FORCE) {
    strcat(buf, "+");
  } else if (format->format_sign == FORMAT_SIGN_SPACE) {
    strcat(buf, " ");
  }

  // Optional prefix
  if (format->format_prefix == FORMAT_PREFIX_AUTO) {
    strcat(buf, "0x");
  }

  // Convert number to hex string
  char num_buf[64];
  hexstrfuint(val, num_buf); // user-provided
  strcat(buf, num_buf);

  return buf;
}

/* Single char */
char *format_char(struct format *format, char *buf, char val) {
  (void)format; // ignore
  buf[0] = val;
  buf[1] = '\0';
  return buf;
}

/* Simple string copy */
char *format_str(struct format *format, char *buf, const char *val) {
  (void)format; // ignore
  buf[0] = '\0';
  strcat(buf, val); // assume val is null-terminated
  return buf;
}

/* Pointer as hex */
char *format_ptr(struct format *format, char *buf, const void *val) {
  buf[0] = '\0';

  if (format->format_prefix == FORMAT_PREFIX_AUTO) {
    strcat(buf, "*0x");
  }

  char num_buf[64];
  hexstrfuint((uint64_t)val, num_buf);
  strcat(buf, num_buf);

  return buf;
}

/* Binary format: 0101001010001 */
char *format_binary(struct format *format, char *buf, uint64_t val) {
  buf[0] = '\0';

  // Optional sign
  if (format->format_sign == FORMAT_SIGN_FORCE) {
    strcat(buf, "+");
  } else if (format->format_sign == FORMAT_SIGN_SPACE) {
    strcat(buf, " ");
  }

  // Convert number to binary string
  char num_buf[64];
  binstrfuint(val, num_buf); // user-provided
  strcat(buf, num_buf);

  return buf;
}

char *apply_format_generic(struct format *format, char *ret_buf,
                           size_t ret_buf_len, va_list *args) {
  // 1) Make a temp buffer for the raw numeric/string data (before
  // justification).
  //    Let's assume 256 is large enough for your typical usage.
  char temp[256];
  temp[0] = '\0';

  int64_t arg_i;
  uint64_t arg_u;
  char arg_c;
  char *arg_s;
  void *arg_p;

  switch (format->format_type) {
  case FORMAT_TYPE_INT:
    format_int(format, temp, get_int_arg(args));
    break;
  case FORMAT_TYPE_UINT:
    format_uint(format, temp, get_uint_arg(args));
    break;
  case FORMAT_TYPE_HEX:
    format_hex(format, temp, get_uint_arg(args));
    break;
  case FORMAT_TYPE_CHAR:
    format_char(format, temp, get_char_arg(args));
    break;
  case FORMAT_TYPE_STR:
    format_str(format, temp, get_str_arg(args));
    break;
  case FORMAT_TYPE_PTR:
    format_ptr(format, temp, get_ptr_arg(args));
    break;
  case FORMAT_TYPE_BINARY:
    format_binary(format, temp, get_uint_arg(args));
    break;
  default:
    ret_buf[0] = '\0';
    return ret_buf;
  }

  size_t data_len = strlen(temp);
  size_t width = format->format_width; // e.g. 10
  if (width < data_len)
    width = data_len; // no negative padding

  if (width >= ret_buf_len) {
    // If ret_buf too small, just do a truncated copy or handle error
    width = ret_buf_len - 1;
  }

  size_t pad_len = width - data_len;
  char pad_char = (format->format_left_pad == FORMAT_LEFT_PAD_ZERO) ? '0' : ' ';

  // Clear ret_buf
  ret_buf[0] = '\0';

  if (format->format_justify == FORMAT_JUSTIFY_LEFT) {
    // text first, then pad
    strcat(ret_buf, temp);
    for (size_t i = 0; i < pad_len; i++) {
      size_t len = strlen(ret_buf);
      if (len + 1 < ret_buf_len) {
        ret_buf[len] = pad_char;
        ret_buf[len + 1] = '\0';
      }
    }
  } else {
    // pad first, then text
    for (size_t i = 0; i < pad_len; i++) {
      size_t len = strlen(ret_buf);
      if (len + 1 < ret_buf_len) {
        ret_buf[len] = pad_char;
        ret_buf[len + 1] = '\0';
      }
    }
    strcat(ret_buf, temp);
  }

  return ret_buf;
}

char *format(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char *ret_buf = (char *)alloc_page();

  if (!ret_buf) {
    va_end(args);
    return NULL;
  }
  ret_buf[0] = '\0';

  // Temp buffer for expansions
  char fmt_buf[256];

  int main_index = 0;
  while (fmt[main_index] != '\0') {
    if (fmt[main_index] == '%' && fmt[main_index + 1] == '{') {
      // parse format string between %{ and }
      char format_str[128];
      int fidx = 0;
      main_index += 2;
      while (fmt[main_index] != '\0' && fmt[main_index] != '}') {
        format_str[fidx++] = fmt[main_index++];
      }
      format_str[fidx] = '\0';

      // if main[main_index] == '}', skip it
      if (fmt[main_index] == '}') {
        main_index++;
      }

      // parse into struct
      struct format format;
      format_parse(&format, format_str);

#ifdef DEBUG
      // Debug: dump format
      {
        char dumpbuf[256];
        format_dump(&format, dumpbuf, sizeof(dumpbuf));
        term_puts(dumpbuf);
        term_puts("\n");
      }
#endif

      // get width/precision from args if flagged
      if (format.width_from_args) {
        format.format_width = (uint64_t)va_arg(args, int);
      }
      if (format.precision_from_args) {
        format.format_precision = (uint64_t)va_arg(args, int);
      }

      // apply
      fmt_buf[0] = '\0';
      apply_format_generic(&format, fmt_buf, sizeof(fmt_buf), &args);

      // concatenate the formatted piece
      strcat(ret_buf, fmt_buf);
    } else {
      // copy literal chars
      size_t len = strlen(ret_buf);
      ret_buf[len] = fmt[main_index];
      ret_buf[len + 1] = '\0';
      main_index++;
    }
  }

  va_end(args);
  return ret_buf; // caller can free_page(ret_buf) if desired
}
