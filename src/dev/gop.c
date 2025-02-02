/*
 *  @description: Graphics output protocol.
 *  @author: Queso Fuego.
 */

#include <dev/gop.h>

static UINT32 *backbuffer = NULL;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
static UINTN num_modes = 0;

/*
 *  Returns GOP.
 */

static EFI_GRAPHICS_OUTPUT_PROTOCOL *get_gop(void)
{
  EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_STATUS status;

  status = uefi_call_wrapper(BS->LocateProtocol,
                             3,
                             &gop_guid,
                             NULL,
                             (void **)&gop
  );

  if (EFI_ERROR(status))
  {
    Print(L"ERROR: Graphics output protocol unavailable!\n");
    halt();
  }

  return gop;
}

void clear_backbuffer(void)
{
  // Clear the backbuffer.
  for (UINT32 i = 0; i < gop->Mode->FrameBufferSize/4; ++i)
  {
    backbuffer[i] = 0;
  }
}

void gop_init(void)
{
  EFI_STATUS status;

  Print(L"Detecting Graphics Output Protocol..\n");
  gop = get_gop();
  
  Print(L"Found GOP at address 0x%x, occupying %d pages.\n",
        gop->Mode->FrameBufferBase,
        gop->Mode->FrameBufferSize/4096
  );
  
  backbuffer = AllocatePool(gop->Mode->FrameBufferSize);
  clear_backbuffer();

  // Get available modes.
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
  UINTN sizeof_info, native_mode;

  status = uefi_call_wrapper(gop->QueryMode,
                             4,
                             gop,
                             gop->Mode == NULL ? 0 : gop->Mode->Mode,
                             &sizeof_info,
                             &info
  );
  
  // Get around buggy UEFI firmware.
  if (status == EFI_NOT_STARTED)
  {
    status = uefi_call_wrapper(gop->SetMode, 2, gop, 0);
  }

  if (EFI_ERROR(status))
  {
    Print(L"Failed to get mode count\n");
  }

  num_modes = gop->Mode->MaxMode;
}

/*
 *  Returns the size of the framebuffer
 *  in bytes.
 */

UINT32 gop_get_size(void)
{
  return gop_get_pitch()*gop_get_height();
}

/*
 *  Returns the index into the
 *  framebuffer.
 */

UINT32 gop_get_index(UINT32 x, UINT32 y)
{
  return x+y*(gop_get_pitch()/4);
}

/*
 *  Returns the width of the framebuffer.
 */

UINT32 gop_get_width(void)
{
  return gop->Mode->Info->HorizontalResolution;
}

/*
 *  Returns the height of the framebuffer.
 */

UINT32 gop_get_height(void)
{
  return gop->Mode->Info->VerticalResolution;
}

/*
 *  Returns the address of the backbuffer.
 */

UINT32 *gop_get_addr(void)
{
  return backbuffer;
}

/*
 *  Returns framebuffer pitch.
 */

UINT32 gop_get_pitch(void)
{
  return 4*gop->Mode->Info->PixelsPerScanLine;
}

/*
 *  Writes the backbuffer from
 *  postion (start_x,start_y) to
 *  the main buffer until (end_x, end_y)
 */

void gop_swap_buffers_at(UINT32 start_x, UINT32 start_y,
                         UINT32 end_x, UINT32 end_y)
{
  UINT32 *fb = (UINT32 *)gop->Mode->FrameBufferBase;


  for (UINT32 y = start_y; y < end_y; ++y)
  {
    for (UINT32 x = start_x; x < end_x; ++x)
    {
      UINT32 pixel = backbuffer[gop_get_index(x, y)];
      fb[gop_get_index(x, y)] = pixel;
    }
  }
}

void gop_swap_buffers(void)
{
  UINT32 *fb = (UINT32 *)gop->Mode->FrameBufferBase;

  for (UINT32 i = 0; i < gop->Mode->FrameBufferSize/4; ++i)
  {
    fb[i] = backbuffer[i];
  }
}

void gop_next_mode(void)
{
  static UINTN next_mode = 1;
  uefi_call_wrapper(gop->SetMode, 2, gop, next_mode++);

  if (next_mode >= num_modes)
  {
    next_mode = 0;
  }

  FreePool(backbuffer);
  backbuffer = AllocatePool(gop->Mode->FrameBufferSize);
  clear_backbuffer();
}
