#include "awake.hpp"
#include "utils.hpp"

const char *color_to_html(const char *color);
const char *make_char_html_safe(const char c);

void convert_and_write_string_to_file(const char *str, const char *path) {
  FILE *fl;
  if (!(fl = fopen(path, "w"))) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Cannot open file %s for write", path);
    return;
  }

  fprintf(fl, "<HTML><BODY bgcolor=#11191C><PRE>");
  // convert lines here

  char result[MAX_STRING_LENGTH * 10] = { '\0' };

  for (const char *ptr = str; *ptr; ptr++) {
    // It's a color. Pull it out.
    if (*ptr == '^') {
      char color[8] = {0};

      size_t max_sz = 2;
      if (*(ptr + 1) == '[' && strlen(ptr) >= sizeof(color) - 1) {
        max_sz = sizeof(color) - 1;
      }
    
      for (size_t idx = 0; idx < max_sz; idx++)
        color[idx] = *(ptr++);
      ptr--;
      
      strlcat(result, color_to_html(color), sizeof(result));
    } else {
      strlcat(result, make_char_html_safe(*ptr), sizeof(result));
      if (*ptr == '\n') {
        fprintf(fl, "%s", result);
        result[0] = '\0';
      }
    }
  }

  fprintf(fl, "</PRE></BODY></HTML>\r\n");
  fclose(fl);
}

#define CONVERT_TO_HEX(character, value) case character: strlcpy(color_to_convert, value, sizeof(color_to_convert)); break;
const char *color_to_html(const char *color) {
  static char html_version[MAX_STRING_LENGTH * 4];
  *html_version = '\0';

  if (!color || !*color || *color != '^') {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid color code '%s' to color_to_html.", color ? color : "(NULL)");
    return html_version;
  }

  char color_to_convert[MAX_INPUT_LENGTH];
  strlcpy(color_to_convert, color, sizeof(color_to_convert));

  // It's an ANSI or extended color. Convert it to an xterm256 color for sending.
  const char *ptr = (color_to_convert + 1);
  switch (*ptr) {
    CONVERT_TO_HEX('r', "^[F300]");
    CONVERT_TO_HEX('R', "^[F500]");
    CONVERT_TO_HEX('g', "^[F030]");
    CONVERT_TO_HEX('G', "^[F050]");
    CONVERT_TO_HEX('y', "^[F330]");
    CONVERT_TO_HEX('Y', "^[F550]");
    CONVERT_TO_HEX('b', "^[F025]");
    CONVERT_TO_HEX('B', "^[F035]");
    CONVERT_TO_HEX('m', "^[F303]");
    CONVERT_TO_HEX('M', "^[F505]");
    CONVERT_TO_HEX('c', "^[F033]");
    CONVERT_TO_HEX('C', "^[F055]");
    CONVERT_TO_HEX('n', "^[F444]");
    CONVERT_TO_HEX('N', "^[F444]");
    CONVERT_TO_HEX('w', "^[F444]");
    CONVERT_TO_HEX('W', "^[F555]");
    CONVERT_TO_HEX('a', "^[F014]");
    CONVERT_TO_HEX('A', "^[F025]");
    CONVERT_TO_HEX('j', "^[F031]");
    CONVERT_TO_HEX('J', "^[F052]");
    CONVERT_TO_HEX('e', "^[F140]");
    CONVERT_TO_HEX('E', "^[F250]");
    CONVERT_TO_HEX('l', "^[F111]");
    CONVERT_TO_HEX('L', "^[F222]");
    CONVERT_TO_HEX('o', "^[F520]");
    CONVERT_TO_HEX('O', "^[F530]");
    CONVERT_TO_HEX('p', "^[F301]");
    CONVERT_TO_HEX('P', "^[F502]");
    CONVERT_TO_HEX('t', "^[F210]");
    CONVERT_TO_HEX('T', "^[F321]");
    CONVERT_TO_HEX('v', "^[F104]");
    CONVERT_TO_HEX('V', "^[F205]");
    case '^':
      strlcpy(html_version, "^", sizeof(html_version));
      return html_version;
    case '[':
      // It already was an xterm color. Do nothing.
      if (strlen(color) == 7) {
        break;
      }
      // Otherwise:
      // fall through
    default:
      // Return what we were given.
      strlcpy(html_version, color, sizeof(html_version));
      return html_version;
  }

  // Once we've gotten here, the color is an xterm color.
  if (*(color_to_convert + 1) == '[' && (*(color_to_convert + 2) == 'F' || *(color_to_convert + 2) == 'f')) {
    unsigned int r_base = *(color_to_convert + 3) - '0';
    unsigned int g_base = *(color_to_convert + 4) - '0';
    unsigned int b_base = *(color_to_convert + 5) - '0';

    if (r_base < 0 || r_base > 5 || g_base < 0 || g_base > 5 || b_base < 0 || b_base > 5) {
      strlcpy(html_version, color, sizeof(html_version));
      return html_version;
    }

    // Go from a 6-value color (0-5) to a 256-value color (0-255).
    unsigned int r_val = r_base * 255 / 5;
    unsigned int g_val = g_base * 255 / 5;
    unsigned int b_val = b_base * 255 / 5;
    snprintf(html_version, sizeof(html_version), "<span style=\"color:#%02X%02X%02X\">", r_val, g_val, b_val);
#ifdef HTML_DEBUG
    snprintf(ENDOF(html_version), sizeof(html_version) - strlen(html_version), "<!-- was %s aka %u %u %u == %u %u %u -->",
             color,
             r_base, g_base, b_base,
             r_val, g_val, b_val);
#endif
    return html_version;
  }

  strlcpy(html_version, color, sizeof(html_version));
  return html_version;
}
#undef CONVERT_TO_HEX

const char *make_char_html_safe(const char c) {
  static char result[10];
  switch (c) {
    case '<':
      strlcpy(result, "&lt;", sizeof(result));
      break;
    case '>':
      strlcpy(result, "&gt;", sizeof(result));
      break;
    case '(':
      strlcpy(result, "&lpar;", sizeof(result));
      break;
    case ')':
      strlcpy(result, "&rpar;", sizeof(result));
      break;
    default:
      *result = c;
      *(result + 1) = '\0';
      break;
  }
  return result;
}
