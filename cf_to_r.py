import sys
import re

def extract_hex_values(data):
    """Extract hex values from the C array."""
    # Remove all non-hex characters and split by commas
    hex_values = re.findall(r'0x([0-9A-Fa-f]{2})', data)
    return [int(x, 16) for x in hex_values]

def bytes_to_font_text(bytes_data):
    """Convert bytes to font text representation."""
    result = []

    # Each character is 16 bytes (16 rows of 8 pixels)
    num_chars = len(bytes_data) // 16

    for char_idx in range(num_chars):
        result.append(f"{char_idx}:")
        result.append("")  # Empty line after header

        # Get the 16 bytes for this character
        char_bytes = bytes_data[char_idx * 16:(char_idx + 1) * 16]

        # Convert each byte to a row of dots and spaces
        for byte in char_bytes:
            # Convert byte to binary, remove '0b' prefix, and pad to 8 bits
            binary = format(byte, '08b')
            # Replace 1s with dots and 0s with spaces
            row = binary.replace('1', '.').replace('0', ' ')
            result.append(row)

        result.append("")  # Empty line after character
        result.append("")  # Another empty line for spacing

    return "\n".join(result)

def main():
    if len(sys.argv) > 1:
        # Read from file if provided
        with open(sys.argv[1], 'r') as f:
            data = f.read()
    else:
        # Read from stdin
        data = sys.stdin.read()

    bytes_data = extract_hex_values(data)
    font_text = bytes_to_font_text(bytes_data)
    print(font_text)

if __name__ == "__main__":
    main()
