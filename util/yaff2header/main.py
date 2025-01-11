import re

def parse_yaff(filename):
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    font_properties = {}
    characters = []
    i = 0
    total_lines = len(lines)

    # Parse header
    while i < total_lines:
        line = lines[i].strip()
        if re.match(r'^(u\+[0-9A-Fa-f]+|0x[0-9A-Fa-f]+):$', line):
            break
        elif ':' in line:
            key, value = map(str.strip, line.split(':', 1))
            font_properties[key.lower()] = value
        i += 1

    # Parse characters
    while i < total_lines:
        # Skip empty lines
        while i < total_lines and lines[i].strip() == '':
            i += 1

        # Parse code points
        codepoints = []
        while i < total_lines:
            line = lines[i].strip()
            if re.match(r'^(u\+[0-9A-Fa-f]+|0x[0-9A-Fa-f]+):$', line):
                codepoints.append(line.rstrip(':'))
                i += 1
            else:
                break

        if not codepoints:
            i += 1
            continue

        # Skip empty lines after codepoints
        while i < total_lines and lines[i].strip() == '':
            i += 1

        # Parse bitmap or '-' (empty character)
        bitmap = []
        properties = {}
        while i < total_lines:
            line = lines[i].rstrip('\n')
            stripped_line = line.strip()

            if re.match(r'^[\.\@\ ]+$', stripped_line) or stripped_line == '-':
                if stripped_line == '-':
                    bitmap = []  # Empty bitmap
                else:
                    bitmap.append(stripped_line)
                i += 1
            elif ':' in stripped_line:
                # Parse character properties
                key, value = map(str.strip, stripped_line.split(':', 1))
                properties[key.lower()] = value
                i += 1
            elif stripped_line == '':
                # Blank line, possible end of current character
                i += 1
                break
            else:
                # Next character or unexpected line
                break

        character = {
            'codepoints': codepoints,
            'bitmap': bitmap,
            'properties': properties
        }
        characters.append(character)

    return font_properties, characters

def codepoint_to_char(codepoint_str):
    """Convert codepoint string to integer and character (if possible)."""
    if codepoint_str.startswith('u+'):
        codepoint = int(codepoint_str[2:], 16)
    elif codepoint_str.startswith('0x'):
        codepoint = int(codepoint_str[2:], 16)
    else:
        codepoint = int(codepoint_str)

    try:
        char = chr(codepoint)
        # Include printable characters
        if char.isprintable():
            return codepoint, char
        else:
            return codepoint, None
    except ValueError:
        return codepoint, None

def generate_c_header(font_properties, characters, output_filename):
    # Determine maximum dimensions considering left and right bearings
    max_bitmap_width = int(font_properties.get('bounding-box', '0 0').split()[0])
    max_bitmap_height = int(font_properties.get('bounding-box', '0 0').split()[1])

    max_total_width = 0  # To store the maximum total width (bitmap + bearings)
    for char in characters:
        properties = char['properties']
        left_bearing = int(properties.get('left-bearing', '0'))
        right_bearing = int(properties.get('right-bearing', '0'))
        bitmap_width = max(len(line) for line in char['bitmap']) if char['bitmap'] else 0
        total_width = left_bearing + bitmap_width + right_bearing
        if total_width > max_total_width:
            max_total_width = total_width

    header_lines = []
    header_lines.append('#ifndef FONT_H')
    header_lines.append('#define FONT_H')
    header_lines.append('')
    header_lines.append('#include <stdint.h>')
    header_lines.append('')
    header_lines.append(f'struct Font {{')
    header_lines.append(f'    uint32_t codepoint;')
    header_lines.append(f'    char code[{max_bitmap_height}][{max_total_width + 1}];  // +1 for null terminator')
    header_lines.append(f'}};')
    header_lines.append('')
    header_lines.append(f'struct Font font[] = {{')

    for char in characters:
        codepoints = char['codepoints']
        bitmap = char['bitmap']
        properties = char['properties']

        # Use the first codepoint for the codepoint value
        codepoint_str = codepoints[0]
        codepoint_value, char_literal = codepoint_to_char(codepoint_str)
        codepoint_hex = f'0x{codepoint_value:X}'
        codepoint_comment = ''
        if char_literal:
            codepoint_comment = f'/* \'{char_literal}\' */'

        # Start character struct
        header_lines.append(f'    {{{codepoint_hex}, {codepoint_comment}')

        # Get bearings
        left_bearing = int(properties.get('left-bearing', '0'))
        right_bearing = int(properties.get('right-bearing', '0'))

        # Add bitmap
        header_lines.append('    {')
        bitmap_height = len(bitmap)
        bitmap_width = max(len(line) for line in bitmap) if bitmap else 0

        # Adjust bitmap lines with bearings and pad to max_total_width
        for y in range(max_bitmap_height):
            if y < bitmap_height:
                line = bitmap[y]
                # Replace '@' with '#' and '.' with ' '
                line = line.replace('@', '#').replace('.', ' ')
                # Add left and right bearings
                line = ' ' * left_bearing + line + ' ' * right_bearing
            else:
                # Empty line with total width
                line = ' ' * (left_bearing + bitmap_width + right_bearing)
            # Pad line to max_total_width
            line = line.ljust(max_total_width, ' ')
            header_lines.append(f'        "{line}",')
        header_lines.append('    },')
        header_lines.append('    },')

    header_lines.append('};')
    header_lines.append('')
    header_lines.append('#endif // FONT_H')

    # Write to file
    with open(output_filename, 'w', encoding='utf-8') as f:
        f.write('\n'.join(header_lines))

    print(f"C header file '{output_filename}' generated successfully.")

if __name__ == '__main__':
    # Example usage:
    # Replace 'font.yaff' with your YAFF font file
    # The output will be written to 'font.h'
    yaff_filename = 'font.yaff'
    output_filename = 'font.h'

    font_properties, characters = parse_yaff(yaff_filename)
    generate_c_header(font_properties, characters, output_filename)
