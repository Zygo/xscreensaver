/* xscreensaver, Copyright (c) 2014 Dave Odell <dmo2118@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 * 
 * The problem:
 * 
 *   - OSX 10.5 and earlier require .saver bundles to not use GC.
 *   - OSX 10.6 require .saver bundles to use GC.
 *   - OSX 10.7 and later require .saver bundles to not use GC.
 * 
 * So the way to build a portable .saver is to build it with "GC optional",
 * via "-fobjc-gc" on the x86-64 architecture.
 * 
 * But XCode 5.x on OSX 10.9 no longer supports building executables
 * that support GC, even optionally.  So there's no way to make XCode
 * 5.x create a .saver bundle that will work on OSX 10.6. Though it
 * will work on 10.5!
 * 
 * The fix: after compiling, hand-hack the generated binary to tag the
 * x86-64 arch with the OBJC_IMAGE_SUPPORTS_GC flag.
 * 
 * Specifically, OR the __DATA,__objc_imageinfo section with
 * "00 00 00 00 02 00 00 00"; normally this section is all zeros.
 * The __objc_imageinfo section corresponds to struct objc_image_info in:
 * http://www.opensource.apple.com/source/objc4/objc4-551.1/runtime/objc-private.h
 * You can use "otool -o Interference.saver/Contents/MacOS/Interference"
 * or "otool -s __DATA __objc_imageinfo Interference" to look at the
 * section.
 *
 * This means that the binary is marked as supporting GC, but there
 * are no actual GC-supporting write barriers compiled in!  So does it
 * actually ever GC?  Yes, apparently it does.  Apparently what's
 * going on is that incremental-GCs are doing nothing, but full-GCs
 * still collect ObjC objects properly.
 *
 * Mad Science!
 *
 * In the xscreensaver build process, the "enable_gc" target is a
 * dependency of "libjwxyz" (so that it gets built first) and is
 * invoked by "update-info-plist.pl" (so that it gets run on every
 * saver).
 */

#include <assert.h>
#include <CoreFoundation/CFByteOrder.h>
#include <fcntl.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define BOUNDS_CHECK(ptr, end) \
  ((const void *)((ptr) + 1) <= (const void *)(end))

#define BOUNDS_CHECK_PRINT(ptr, end) \
  (BOUNDS_CHECK(ptr, end) ? 1 : (_got_eof(), 0))

/*
  This part is lifted from objc-private.h, because it's not present on
  most OS X systems.
  http://www.opensource.apple.com/source/objc4/objc4-551.1/runtime/objc-private.h
 */

typedef struct {
    uint32_t version; // currently 0
    uint32_t flags;
} objc_image_info;

// masks for objc_image_info.flags
#define OBJC_IMAGE_IS_REPLACEMENT (1<<0)
#define OBJC_IMAGE_SUPPORTS_GC (1<<1)
#define OBJC_IMAGE_REQUIRES_GC (1<<2)
#define OBJC_IMAGE_OPTIMIZED_BY_DYLD (1<<3)
#define OBJC_IMAGE_SUPPORTS_COMPACTION (1<<4)  // might be re-assignable

/* End objc-private.h excerpt. */

static void
_got_eof()
{
  fputs("Error: Unexpected EOF\n", stderr);
}

/* This will probably only ever run on OS X, so CoreFoundation is used here. */

static inline uint32_t 
_be_u32(uint32_t x) /* Big Endian _ Unsigned int 32-bit */
{
  return (uint32_t)CFSwapInt32BigToHost(x);
}

static inline uint32_t
_le_u32(uint32_t x) /* Little Endian _ Unsigned int 32-bit */
{
  return (uint32_t)CFSwapInt32LittleToHost(x);
}

static inline uint32_t
_le_u64(uint64_t x) /* Little Endian _ Unsigned int 64-bit */
{
  return (uint32_t)CFSwapInt64LittleToHost(x);
}

static int 
_handle_x86_64(void *exec, void *exec_end)
{
  const uint32_t *magic = exec;

  if(!BOUNDS_CHECK_PRINT(magic, exec_end))
    return EXIT_FAILURE;
	
  if(*magic != _le_u32(MH_MAGIC_64))
    {
      fputs("Error: Unknown magic number on Mach header.\n", stderr);
      return EXIT_FAILURE;
    }

  /* Mach headers can be little-endian or big-endian. */

  const struct mach_header_64 *hdr = (const struct mach_header_64 *)magic;
  if(!BOUNDS_CHECK_PRINT(hdr, exec_end))
    return EXIT_FAILURE;

  if(hdr->cputype != _le_u32(CPU_TYPE_X86_64))
    {
      fputs("Error: Unexpected CPU type on Mach header.\n", stderr);
      return EXIT_FAILURE;
    }
	
  /* I may have missed a few _le_u32 calls, so watch out on PowerPC (heh). */
	
  if((const uint8_t *)hdr + _le_u32(hdr->sizeofcmds) >
     (const uint8_t *)exec_end)
    {
      _got_eof();
      return EXIT_FAILURE;
    }

  const struct load_command *load_cmd = (const struct load_command *)(hdr + 1);
  const void *cmds_end = (const uint8_t *)load_cmd + hdr->sizeofcmds;
	
  for(unsigned i = 0; i != _le_u32(hdr->ncmds); ++i)
    {
      if(!BOUNDS_CHECK_PRINT(load_cmd, cmds_end))
        return EXIT_FAILURE;

      const struct load_command *next_load_cmd =
        (const struct load_command *)((const uint8_t *)load_cmd +
                                      _le_u32(load_cmd->cmdsize));

      if(load_cmd->cmd == _le_u32(LC_SEGMENT_64))
        {
          const struct segment_command_64 *seg_cmd =
            (const struct segment_command_64 *)load_cmd;
          if(!BOUNDS_CHECK_PRINT(seg_cmd, cmds_end))
            return EXIT_FAILURE;
			
          if(!strcmp(seg_cmd->segname, "__DATA"))
            {
              const struct section_64 *sect =
                (const struct section_64 *)(seg_cmd + 1);
              for(unsigned j = 0; j != _le_u32(seg_cmd->nsects); ++j)
                {
                  if(!BOUNDS_CHECK_PRINT(&sect[j], next_load_cmd))
                    return EXIT_FAILURE;

                  if(strcmp(sect[j].segname, "__DATA"))
                    fprintf(stderr,
                            "Warning: segment name mismatch in __DATA,%.16s\n",
                            sect[j].sectname);
						
                  if(!memcmp(sect[j].sectname, "__objc_imageinfo", 16))
                    { /* No null-terminator here. */
                      if(_le_u64(sect[j].size) < sizeof(objc_image_info))
                        {
                          fputs("__DATA,__objc_imageinfo too small.\n",
                                stderr);
                          return EXIT_FAILURE;
                        }
						
                      /*
                        Not checked:
                        - Overlapping segments.
                        - Segments overlapping the load commands.
                      */
						
                      objc_image_info *img_info = (objc_image_info *)
                        ((uint8_t *)exec + _le_u64(sect[j].offset));
						
                      if(!BOUNDS_CHECK_PRINT(img_info, exec_end))
                        return EXIT_FAILURE;
						
                      if(img_info->version != 0)
                        {
                          fprintf(
                                  stderr,
                                  "Error: Unexpected version for "
                                  "__DATA,__objc_imageinfo section. "
                                  "Expected 0, got %d\n",
                                  _le_u32(img_info->version));
                          return EXIT_FAILURE;
                        }
						
                      if(img_info->flags &
                         _le_u32(OBJC_IMAGE_REQUIRES_GC |
                                 OBJC_IMAGE_SUPPORTS_GC))
                        {
                          fputs("Warning: Image already supports GC.\n",
                                stderr);
                          return EXIT_SUCCESS;
                        }

                      /* Finally, do the work. */
                      img_info->flags |= _le_u32(OBJC_IMAGE_SUPPORTS_GC);
                      return EXIT_SUCCESS;
                    }
                }
            }
        }
		
      load_cmd = next_load_cmd;
    }
	
  if((const void *)load_cmd > cmds_end)
    {
      _got_eof();
      return EXIT_FAILURE;
    }
	
  assert(load_cmd == cmds_end);
	
  fputs("Error: __DATA,__objc_imageinfo not found.\n", stderr);
  return EXIT_FAILURE;
}

int
main(int argc, const char **argv)
{
  if(argc != 2)
    {
      fprintf(stderr, "Usage: %s executable\n", argv[0]);
      return EXIT_FAILURE;
    }
	
  const char *exec_path = argv[1];

  int fd = open(exec_path, O_RDWR | O_EXLOCK);
	
  if(fd < 0)
    {
      perror(exec_path);
      return EXIT_FAILURE;
    }

  int result = EXIT_FAILURE;
	
  struct stat exec_stat;
  if(fstat(fd, &exec_stat) < 0)
    {
      perror("fstat");
      exit (1);
    }
  else
    {
      if(!(exec_stat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        {
          fprintf(stderr, "Warning: %s is not executable.\n", exec_path);
          exit (1);
        }
		
      assert(exec_stat.st_size >= 0);
		
      /*
        TODO (technically): mmap(2) can throw signals if somebody unplugs
        the file system. In such situations, a signal handler
        should be used to ensure sensible recovery.
      */

      void *exec = NULL;
		
      if(exec_stat.st_size)
        {
          exec = mmap(NULL, exec_stat.st_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
          if(!exec)
            perror("mmap");
        }

      if(exec || !exec_stat.st_size)
        {
          const void *exec_end = (const char *)exec + exec_stat.st_size;
			
          const uint32_t *magic = exec;

          if(BOUNDS_CHECK_PRINT(magic, exec_end))
            {
              if(*magic == _be_u32(FAT_MAGIC))
                {
                  struct fat_header *hdr = (struct fat_header *)magic;
                  if(BOUNDS_CHECK_PRINT(hdr, exec_end))
                    {
                      uint32_t nfat_arch = _be_u32(hdr->nfat_arch);
                      const struct fat_arch *arch =
                        (const struct fat_arch *)(hdr + 1);

                      unsigned i = 0;
                      for(;;)
                        {
                          if(i == nfat_arch)
                            {
                              /* This could be done for other architectures. */
                              fputs("Error: x86_64 architecture not found.\n",
                                    stderr);
                              exit (1);
                              break;
                            }
							
                          if(!BOUNDS_CHECK_PRINT(&arch[i], exec_end))
                            break;

                          if(arch[i].cputype == _be_u32(CPU_TYPE_X86_64))
                            {
                              uint8_t *obj_begin = 
                                (uint8_t *)exec + _be_u32(arch[i].offset);
                              result = _handle_x86_64(obj_begin,
                                                      obj_begin +
                                                      _be_u32(arch[i].size));
                              break;
                            }

                          ++i;
                        }
                    }
                }
              else
                {
                  fprintf(stderr,
                       "Error: %s is not a recognized Mach binary format.\n",
                          exec_path);
                  exit (1);
                }
            }
			
          munmap(exec, exec_stat.st_size);
        }
    }
	
  close(fd);
	
  return result;
}
