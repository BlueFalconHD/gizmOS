# graphics planning

gizmOS's current graphics solution is a framebuffer with a simple API for drawing pixels and arrays of pixels. While this has been enough for basic things, and is perfect for boot-time logging, the raw framebuffer by itself doesn't provide enough flexibility or performance.

To solve this problem, a more versatile graphics solution running on top of or beside the framebuffer is needed. This will be implemented in several stages.

## graphics stage 1

- framebuffer

## graphics stage 2 - HERE NOW

- surfaces
  - create a surface X and Y offset from origin, width and height, and a Z-index
    - surfaces can be resized and repositioned, their Z-index can be changed
  - draw to surface buffer
    - dirty rectangle tracking: up to 8 then coalescese

- draw surfaces to back buffer in Z-order with respect to Z-offset and drawing stuff
- copy dirty rectangles from back buffer to framebuffer

## graphics stage 3

- hardware acceleration
  - blit dirty rects straight from surface buffers to gpu buffer, halves the number of copies

## graphics stage 4??
- ???
