import sys
import re

def parse_font_text(text):
    """Parse the text representation of the font and convert to bytes."""
    bytes_data = []

    # Split by character blocks (which start with a number followed by a colon)
    char_blocks = re.split(r'^\d+:', text, flags=re.MULTILINE)

    # Skip the first entry which is empty
    if char_blocks and not char_blocks[0].strip():
        char_blocks = char_blocks[1:]

    for i, block in enumerate(char_blocks):
        if not block.strip():
            continue

        # Extract the rows containing the character representation
        rows = [line for line in block.strip().split('\n') if line.strip()]

        # Make sure we have exactly 16 rows (padding with empty rows if needed)
        rows = rows[:16]
        rows.extend([''] * (16 - len(rows)))

        for row in rows:
            # Convert row of dots and spaces to binary, then to integer
            if not row:
                # Empty row, all pixels are off
                binary = '00000000'
            else:
                # Replace dots with 1s and anything else with 0s
                binary = ''.join('1' if char == '.' else '0' for char in row.ljust(8)[:8])

            # Convert binary to integer
            byte_value = int(binary, 2)
            bytes_data.append(byte_value)

    return bytes_data

def format_c_array(bytes_data):
    """Format bytes as a C array."""
    # Format each byte as a hex string
    hex_strings = [f"0x{byte:02x}" for byte in bytes_data]

    # Format the array with 12 values per line
    lines = []
    for i in range(0, len(hex_strings), 12):
        line = "  " + ", ".join(hex_strings[i:i+12]) + ","
        lines.append(line)

    # Add C array wrapper
    array_code = "unsigned char font_data[] = {\n" + "\n".join(lines) + "\n};"
    return array_code

def main():
    if len(sys.argv) > 1:
        # Read from file if provided
        with open(sys.argv[1], 'r') as f:
            text = f.read()
    else:
        # Read from stdin
        text = sys.stdin.read()

    bytes_data = parse_font_text(text)
    c_array = format_c_array(bytes_data)
    print(c_array)

if __name__ == "__main__":
    main()
