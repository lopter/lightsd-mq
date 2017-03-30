# HG changeset patch
# Parent  bb7a1425080bc272456650675e5e03896ca21d54
cmake: update UseLatex.cmake to 2.4.2

diff --git a/.hgignore b/.hgignore
--- a/.hgignore
+++ b/.hgignore
@@ -3,3 +3,4 @@
 ^build
 ^pcaps
 .*\.egg-info$
+\.DS_Store$
diff --git a/CMakeScripts/UseLATEX.cmake b/CMakeScripts/UseLATEX.cmake
--- a/CMakeScripts/UseLATEX.cmake
+++ b/CMakeScripts/UseLATEX.cmake
@@ -1,6 +1,6 @@
 # File: UseLATEX.cmake
 # CMAKE commands to actually use the LaTeX compiler
-# Version: 2.3.2
+# Version: 2.4.2
 # Author: Kenneth Moreland <kmorel@sandia.gov>
 #
 # Copyright 2004, 2015 Sandia Corporation.
@@ -61,15 +61,15 @@
 #       so all input files are copied from the source directory to the
 #       output directory.  This includes the target tex file, any tex file
 #       listed with the INPUTS option, the bibliography files listed with
-#       the BIBFILES option, and any .cls, .bst, and .clo files found in
-#       the current source directory.  Images found in the IMAGE_DIRS
-#       directories or listed by IMAGES are also copied to the output
-#       directory and converted to an appropriate format if necessary.  Any
-#       tex files also listed with the CONFIGURE option are also processed
-#       with the CMake CONFIGURE_FILE command (with the @ONLY flag).  Any
-#       file listed in CONFIGURE but not the target tex file or listed with
-#       INPUTS has no effect. DEPENDS can be used to specify generated files
-#       that are needed to compile the latex target.
+#       the BIBFILES option, and any .cls, .bst, .clo, .sty, .ist, and .fd
+#       files found in the current source directory.  Images found in the
+#       IMAGE_DIRS directories or listed by IMAGES are also copied to the
+#       output directory and converted to an appropriate format if necessary.
+#       Any tex files also listed with the CONFIGURE option are also processed
+#       with the CMake CONFIGURE_FILE command (with the @ONLY flag).  Any file
+#       listed in CONFIGURE but not the target tex file or listed with INPUTS
+#       has no effect. DEPENDS can be used to specify generated files that are
+#       needed to compile the latex target.
 #
 #       The following targets are made. The name prefix is based off of the
 #       base name of the tex file unless TARGET_NAME is specified. If
@@ -115,6 +115,37 @@
 #
 # History:
 #
+# 2.4.2 Fix an issue where new versions of ImageMagick expect the order of
+#       options in command line execution of magick/convert. (See, for
+#       example, http://www.imagemagick.org/Usage/basics/#why.)
+#
+# 2.4.1 Add ability to dump LaTeX log file when using batch mode. Batch
+#       mode suppresses most output, often including error messages. To
+#       make sure critical error messages get displayed, show the full log
+#       on failures.
+#
+# 2.4.0 Remove "-r 600" from the default PDFTOPS_CONVERTER_FLAGS. The -r flag
+#       is available from the Poppler version of pdftops, but not the Xpdf
+#       version.
+#
+#       Fix an issue with the flags for the different programs not being
+#       properly separated.
+#
+#       Fix an issue on windows where the = character is not allowed for
+#       ps2pdf arguments.
+#
+#       Change default arguments for latex and pdflatex commands. Makes the
+#       output more quiet and prints out the file/line where errors occur.
+#       (Thanks to Nikos Koukis.)
+#
+#       After a LaTeX build, check the log file for warnings that are
+#       indicative of problems with the build.
+#
+#       Remove support for latex2html. Instead, use the htlatex program.
+#       This is now part of TeX Live and most other distributions. It also
+#       behaves much more like the other LaTeX programs. Also fixed some
+#       nasty issues with the htlatex arguments.
+#
 # 2.3.2 Declare LaTeX input files as sources for targets so that they show
 #       up in IDEs like QtCreator.
 #
@@ -374,6 +405,36 @@
 #############################################################################
 # Functions that perform processing during a LaTeX build.
 #############################################################################
+function(latex_execute_latex)
+  if(NOT LATEX_TARGET)
+    message(SEND_ERROR "Need to define LATEX_TARGET")
+  endif()
+
+  if(NOT LATEX_WORKING_DIRECTORY)
+    message(SEND_ERROR "Need to define LATEX_WORKING_DIRECTORY")
+  endif()
+
+  if(NOT LATEX_FULL_COMMAND)
+    message(SEND_ERROR "Need to define LATEX_FULL_COMMAND")
+  endif()
+
+  set(full_command_original "${LATEX_FULL_COMMAND}")
+  separate_arguments(LATEX_FULL_COMMAND)
+  execute_process(
+    COMMAND ${LATEX_FULL_COMMAND}
+    WORKING_DIRECTORY ${LATEX_WORKING_DIRECTORY}
+    RESULT_VARIABLE execute_result
+    )
+
+  if(NOT ${execute_result} EQUAL 0)
+    message("LaTeX command failed")
+    message("${full_command_original}")
+    message("Log output:")
+    file(READ ${LATEX_WORKING_DIRECTORY}/${LATEX_TARGET}.log log_output)
+    message(FATAL_ERROR "${log_output}")
+  endif()
+endfunction(latex_execute_latex)
+
 function(latex_makeglossaries)
   # This is really a bare bones port of the makeglossaries perl script into
   # CMake scripting.
@@ -492,10 +553,10 @@
         set(codepage_flags "")
       endif()
 
-      message("${XINDY_COMPILER} ${MAKEGLOSSARIES_COMPILER_FLAGS} ${language_flags} ${codepage_flags} -I xindy -M ${glossary_name} -t ${glossary_log} -o ${glossary_out} ${glossary_in}"
+      message("${XINDY_COMPILER} ${MAKEGLOSSARIES_COMPILER_ARGS} ${language_flags} ${codepage_flags} -I xindy -M ${glossary_name} -t ${glossary_log} -o ${glossary_out} ${glossary_in}"
         )
       exec_program(${XINDY_COMPILER}
-        ARGS ${MAKEGLOSSARIES_COMPILER_FLAGS}
+        ARGS ${MAKEGLOSSARIES_COMPILER_ARGS}
           ${language_flags}
           ${codepage_flags}
           -I xindy
@@ -514,7 +575,7 @@
       if("${xindy_output}" MATCHES "^Cannot locate xindy module for language (.+) in codepage (.+)\\.$")
         message("*************** Retrying xindy with default codepage.")
         exec_program(${XINDY_COMPILER}
-          ARGS ${MAKEGLOSSARIES_COMPILER_FLAGS}
+          ARGS ${MAKEGLOSSARIES_COMPILER_ARGS}
             ${language_flags}
             -I xindy
             -M ${glossary_name}
@@ -525,8 +586,8 @@
       endif()
 
     else()
-      message("${MAKEINDEX_COMPILER} ${MAKEGLOSSARIES_COMPILER_FLAGS} -s ${istfile} -t ${glossary_log} -o ${glossary_out} ${glossary_in}")
-      exec_program(${MAKEINDEX_COMPILER} ARGS ${MAKEGLOSSARIES_COMPILER_FLAGS}
+      message("${MAKEINDEX_COMPILER} ${MAKEGLOSSARIES_COMPILER_ARGS} -s ${istfile} -t ${glossary_log} -o ${glossary_out} ${glossary_in}")
+      exec_program(${MAKEINDEX_COMPILER} ARGS ${MAKEGLOSSARIES_COMPILER_ARGS}
         -s ${istfile} -t ${glossary_log} -o ${glossary_out} ${glossary_in}
         )
     endif()
@@ -547,7 +608,7 @@
   set(nomencl_out ${LATEX_TARGET}.nls)
   set(nomencl_in ${LATEX_TARGET}.nlo)
 
-  exec_program(${MAKEINDEX_COMPILER} ARGS ${MAKENOMENCLATURE_COMPILER_FLAGS}
+  exec_program(${MAKEINDEX_COMPILER} ARGS ${MAKENOMENCLATURE_COMPILER_ARGS}
     ${nomencl_in} -s "nomencl.ist" -o ${nomencl_out}
     )
 endfunction(latex_makenomenclature)
@@ -610,6 +671,49 @@
 
 endfunction(latex_correct_synctex)
 
+function(latex_check_important_warnings)
+  set(log_file ${LATEX_TARGET}.log)
+
+  message("\nChecking ${log_file} for important warnings.")
+  if(NOT LATEX_TARGET)
+    message(SEND_ERROR "Need to define LATEX_TARGET")
+  endif()
+
+  if(NOT EXISTS ${log_file})
+    message("Could not find log file: ${log_file}")
+    return()
+  endif()
+
+  set(found_error)
+
+  # Check for undefined references
+  file(STRINGS ${log_file} reference_warnings REGEX "Reference.*undefined")
+  if(reference_warnings)
+    set(found_error TRUE)
+    message("\nFound missing reference warnings.")
+    foreach(warning ${reference_warnings})
+      message("${warning}")
+    endforeach(warning)
+  endif()
+
+  # Check for overfull
+  file(STRINGS ${log_file} overfull_warnings REGEX "^Overfull")
+  if(overfull_warnings)
+    set(found_error TRUE)
+    message("\nFound overfull warnings. These are indicative of layout errors.")
+    foreach(warning ${overfull_warnings})
+      message("${warning}")
+    endforeach(warning)
+  endif()
+
+  if(found_error)
+    latex_get_filename_component(log_file_path ${log_file} ABSOLUTE)
+    message("\nConsult ${log_file_path} for more information on LaTeX build.")
+  else()
+    message("No known important warnings found.")
+  endif(found_error)
+endfunction(latex_check_important_warnings)
+
 #############################################################################
 # Helper functions for establishing LaTeX build.
 #############################################################################
@@ -645,6 +749,12 @@
     DOC "The pdf to ps converter program from the Poppler package."
     )
 
+  find_program(HTLATEX_COMPILER
+    NAMES htlatex
+    PATHS ${MIKTEX_BINARY_PATH}
+      /usr/bin
+    )
+
   mark_as_advanced(
     LATEX_COMPILER
     PDFLATEX_COMPILER
@@ -656,38 +766,31 @@
     PS2PDF_CONVERTER
     PDFTOPS_CONVERTER
     LATEX2HTML_CONVERTER
+    HTLATEX_COMPILER
     )
 
   latex_needit(LATEX_COMPILER latex)
   latex_wantit(PDFLATEX_COMPILER pdflatex)
+  latex_wantit(HTLATEX_COMPILER htlatex)
   latex_needit(BIBTEX_COMPILER bibtex)
   latex_wantit(BIBER_COMPILER biber)
   latex_needit(MAKEINDEX_COMPILER makeindex)
   latex_wantit(DVIPS_CONVERTER dvips)
   latex_wantit(PS2PDF_CONVERTER ps2pdf)
   latex_wantit(PDFTOPS_CONVERTER pdftops)
-  # MiKTeX calls latex2html htlatex
-  if(NOT ${LATEX2HTML_CONVERTER})
-    find_program(HTLATEX_CONVERTER
-      NAMES htlatex
-      PATHS ${MIKTEX_BINARY_PATH}
-            /usr/bin
-      )
-    mark_as_advanced(HTLATEX_CONVERTER)
-    if(HTLATEX_CONVERTER)
-      set(USING_HTLATEX TRUE CACHE INTERNAL "True when using MiKTeX htlatex instead of latex2html" FORCE)
-      set(LATEX2HTML_CONVERTER ${HTLATEX_CONVERTER}
-        CACHE FILEPATH "htlatex taking the place of latex2html" FORCE)
-    else()
-      set(USING_HTLATEX FALSE CACHE INTERNAL "True when using MiKTeX htlatex instead of latex2html" FORCE)
-    endif()
-  endif()
-  latex_wantit(LATEX2HTML_CONVERTER latex2html)
 
-  set(LATEX_COMPILER_FLAGS "-interaction=nonstopmode"
+  set(LATEX_COMPILER_FLAGS "-interaction=batchmode -file-line-error"
     CACHE STRING "Flags passed to latex.")
   set(PDFLATEX_COMPILER_FLAGS ${LATEX_COMPILER_FLAGS}
     CACHE STRING "Flags passed to pdflatex.")
+  set(HTLATEX_COMPILER_TEX4HT_FLAGS "html"
+    CACHE STRING "Options for the tex4ht.sty and *.4ht style files.")
+  set(HTLATEX_COMPILER_TEX4HT_POSTPROCESSOR_FLAGS ""
+    CACHE STRING "Options for the text4ht postprocessor.")
+  set(HTLATEX_COMPILER_T4HT_POSTPROCESSOR_FLAGS ""
+    CACHE STRING "Options for the t4ht postprocessor.")
+  set(HTLATEX_COMPILER_LATEX_FLAGS ${LATEX_COMPILER_FLAGS}
+    CACHE STRING "Flags passed from htlatex to the LaTeX compiler.")
   set(LATEX_SYNCTEX_FLAGS "-synctex=1"
     CACHE STRING "latex/pdflatex flags used to create synctex file.")
   set(BIBTEX_COMPILER_FLAGS ""
@@ -702,15 +805,26 @@
     CACHE STRING "Flags passed to makenomenclature.")
   set(DVIPS_CONVERTER_FLAGS "-Ppdf -G0 -t letter"
     CACHE STRING "Flags passed to dvips.")
-  set(PS2PDF_CONVERTER_FLAGS "-dMaxSubsetPct=100 -dCompatibilityLevel=1.3 -dSubsetFonts=true -dEmbedAllFonts=true -dAutoFilterColorImages=false -dAutoFilterGrayImages=false -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode -dMonoImageFilter=/FlateEncode"
-    CACHE STRING "Flags passed to ps2pdf.")
-  set(PDFTOPS_CONVERTER_FLAGS -r 600
+  if(NOT WIN32)
+    set(PS2PDF_CONVERTER_FLAGS "-dMaxSubsetPct=100 -dCompatibilityLevel=1.3 -dSubsetFonts=true -dEmbedAllFonts=true -dAutoFilterColorImages=false -dAutoFilterGrayImages=false -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode -dMonoImageFilter=/FlateEncode"
+      CACHE STRING "Flags passed to ps2pdf.")
+  else()
+    # Most windows ports of ghostscript utilities use .bat files for ps2pdf
+    # commands. bat scripts interpret "=" as a special character and separate
+    # those arguments. To get around this, the ghostscript utilities also
+    # support using "#" in place of "=".
+    set(PS2PDF_CONVERTER_FLAGS "-dMaxSubsetPct#100 -dCompatibilityLevel#1.3 -dSubsetFonts#true -dEmbedAllFonts#true -dAutoFilterColorImages#false -dAutoFilterGrayImages#false -dColorImageFilter#/FlateEncode -dGrayImageFilter#/FlateEncode -dMonoImageFilter#/FlateEncode"
+      CACHE STRING "Flags passed to ps2pdf.")
+  endif()
+  set(PDFTOPS_CONVERTER_FLAGS ""
     CACHE STRING "Flags passed to pdftops.")
-  set(LATEX2HTML_CONVERTER_FLAGS ""
-    CACHE STRING "Flags passed to latex2html.")
   mark_as_advanced(
     LATEX_COMPILER_FLAGS
     PDFLATEX_COMPILER_FLAGS
+    HTLATEX_COMPILER_TEX4HT_FLAGS
+    HTLATEX_COMPILER_TEX4HT_POSTPROCESSOR_FLAGS
+    HTLATEX_COMPILER_T4HT_POSTPROCESSOR_FLAGS
+    HTLATEX_COMPILER_LATEX_FLAGS
     LATEX_SYNCTEX_FLAGS
     BIBTEX_COMPILER_FLAGS
     BIBER_COMPILER_FLAGS
@@ -720,10 +834,17 @@
     DVIPS_CONVERTER_FLAGS
     PS2PDF_CONVERTER_FLAGS
     PDFTOPS_CONVERTER_FLAGS
-    LATEX2HTML_CONVERTER_FLAGS
     )
+
+  # Because it is easier to type, the flags variables are entered as
+  # space-separated strings much like you would in a shell. However, when
+  # using a CMake command to execute a program, it works better to hold the
+  # arguments in semicolon-separated lists (otherwise the whole string will
+  # be interpreted as a single argument). Use the separate_arguments to
+  # convert the space-separated strings to semicolon-separated lists.
   separate_arguments(LATEX_COMPILER_FLAGS)
   separate_arguments(PDFLATEX_COMPILER_FLAGS)
+  separate_arguments(HTLATEX_COMPILER_LATEX_FLAGS)
   separate_arguments(LATEX_SYNCTEX_FLAGS)
   separate_arguments(BIBTEX_COMPILER_FLAGS)
   separate_arguments(BIBER_COMPILER_FLAGS)
@@ -733,7 +854,24 @@
   separate_arguments(DVIPS_CONVERTER_FLAGS)
   separate_arguments(PS2PDF_CONVERTER_FLAGS)
   separate_arguments(PDFTOPS_CONVERTER_FLAGS)
-  separate_arguments(LATEX2HTML_CONVERTER_FLAGS)
+
+  # Not quite done. When you call separate_arguments on a cache variable,
+  # the result is written to a local variable. That local variable goes
+  # away when this function returns (which is before any of them are used).
+  # So, copy these variables with local scope to cache variables with
+  # global scope.
+  set(LATEX_COMPILER_ARGS "${LATEX_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(PDFLATEX_COMPILER_ARGS "${PDFLATEX_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(HTLATEX_COMPILER_ARGS "${HTLATEX_COMPILER_LATEX_FLAGS}" CACHE INTERNAL "")
+  set(LATEX_SYNCTEX_ARGS "${LATEX_SYNCTEX_FLAGS}" CACHE INTERNAL "")
+  set(BIBTEX_COMPILER_ARGS "${BIBTEX_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(BIBER_COMPILER_ARGS "${BIBER_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(MAKEINDEX_COMPILER_ARGS "${MAKEINDEX_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(MAKEGLOSSARIES_COMPILER_ARGS "${MAKEGLOSSARIES_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(MAKENOMENCLATURE_COMPILER_ARGS "${MAKENOMENCLATURE_COMPILER_FLAGS}" CACHE INTERNAL "")
+  set(DVIPS_CONVERTER_ARGS "${DVIPS_CONVERTER_FLAGS}" CACHE INTERNAL "")
+  set(PS2PDF_CONVERTER_ARGS "${PS2PDF_CONVERTER_FLAGS}" CACHE INTERNAL "")
+  set(PDFTOPS_CONVERTER_ARGS "${PDFTOPS_CONVERTER_FLAGS}" CACHE INTERNAL "")
 
   find_program(IMAGEMAGICK_CONVERT
     NAMES magick convert
@@ -875,7 +1013,7 @@
     if(PS2PDF_CONVERTER)
       set(require_imagemagick_convert FALSE)
       set(converter ${PS2PDF_CONVERTER})
-      set(convert_flags -dEPSCrop ${PS2PDF_CONVERTER_FLAGS})
+      set(convert_flags -dEPSCrop ${PS2PDF_CONVERTER_ARGS})
     else()
       message(SEND_ERROR "Using postscript files with pdflatex requires ps2pdf for conversion.")
     endif()
@@ -886,7 +1024,7 @@
     if(PDFTOPS_CONVERTER)
       set(require_imagemagick_convert FALSE)
       set(converter ${PDFTOPS_CONVERTER})
-      set(convert_flags -eps ${PDFTOPS_CONVERTER_FLAGS})
+      set(convert_flags -eps ${PDFTOPS_CONVERTER_ARGS})
     else()
       message(STATUS "Consider getting pdftops from Poppler to convert PDF images to EPS images.")
       set(convert_flags ${flags})
@@ -902,17 +1040,24 @@
         message(SEND_ERROR "IMAGEMAGICK_CONVERT set to Window's convert.exe for changing file systems rather than ImageMagick's convert for changing image formats. Please make sure ImageMagick is installed (available at http://www.imagemagick.org). If you have a recent version of ImageMagick (7.0 or higher), use the magick program instead of convert for IMAGEMAGICK_CONVERT.")
       else()
         set(converter ${IMAGEMAGICK_CONVERT})
+        # ImageMagick requires a special order of arguments where resize and
+        # arguments of that nature must be placed after the input image path.
+        add_custom_command(OUTPUT ${output_path}
+          COMMAND ${converter}
+            ARGS ${input_path} ${convert_flags} ${output_path}
+          DEPENDS ${input_path}
+          )
       endif()
     else()
       message(SEND_ERROR "Could not find convert program. Please download ImageMagick from http://www.imagemagick.org and install.")
     endif()
+  else() # Not ImageMagick convert
+    add_custom_command(OUTPUT ${output_path}
+      COMMAND ${converter}
+        ARGS ${convert_flags} ${input_path} ${output_path}
+      DEPENDS ${input_path}
+      )
   endif()
-
-  add_custom_command(OUTPUT ${output_path}
-    COMMAND ${converter}
-      ARGS ${convert_flags} ${input_path} ${output_path}
-    DEPENDS ${input_path}
-    )
 endfunction(latex_add_convert_command)
 
 # Makes custom commands to convert a file to a particular type.
@@ -1157,18 +1302,42 @@
 
 function(add_latex_targets_internal)
   if(LATEX_USE_SYNCTEX)
-    set(synctex_flags ${LATEX_SYNCTEX_FLAGS})
+    set(synctex_flags ${LATEX_SYNCTEX_ARGS})
   else()
     set(synctex_flags)
   endif()
 
   # The commands to run LaTeX.  They are repeated multiple times.
   set(latex_build_command
-    ${LATEX_COMPILER} ${LATEX_COMPILER_FLAGS} ${synctex_flags} ${LATEX_MAIN_INPUT}
+    ${LATEX_COMPILER} ${LATEX_COMPILER_ARGS} ${synctex_flags} ${LATEX_MAIN_INPUT}
     )
+  if(LATEX_COMPILER_ARGS MATCHES ".*batchmode.*")
+    # Wrap command in script that dumps the log file on error. This makes sure
+    # errors can be seen.
+    set(latex_build_command
+      ${CMAKE_COMMAND}
+        -D LATEX_BUILD_COMMAND=execute_latex
+        -D LATEX_TARGET=${LATEX_TARGET}
+        -D LATEX_WORKING_DIRECTORY="${output_dir}"
+        -D LATEX_FULL_COMMAND="${latex_build_command}"
+        -P "${LATEX_USE_LATEX_LOCATION}"
+      )
+  endif()
   set(pdflatex_build_command
-    ${PDFLATEX_COMPILER} ${PDFLATEX_COMPILER_FLAGS} ${synctex_flags} ${LATEX_MAIN_INPUT}
+    ${PDFLATEX_COMPILER} ${PDFLATEX_COMPILER_ARGS} ${synctex_flags} ${LATEX_MAIN_INPUT}
     )
+  if(PDFLATEX_COMPILER_ARGS MATCHES ".*batchmode.*")
+    # Wrap command in script that dumps the log file on error. This makes sure
+    # errors can be seen.
+    set(pdflatex_build_command
+      ${CMAKE_COMMAND}
+        -D LATEX_BUILD_COMMAND=execute_latex
+        -D LATEX_TARGET=${LATEX_TARGET}
+        -D LATEX_WORKING_DIRECTORY="${output_dir}"
+        -D LATEX_FULL_COMMAND="${pdflatex_build_command}"
+        -P "${LATEX_USE_LATEX_LOCATION}"
+      )
+  endif()
 
   if(NOT LATEX_TARGET_NAME)
     set(LATEX_TARGET_NAME ${LATEX_TARGET})
@@ -1267,7 +1436,7 @@
         -D LATEX_TARGET=${LATEX_TARGET}
         -D MAKEINDEX_COMPILER=${MAKEINDEX_COMPILER}
         -D XINDY_COMPILER=${XINDY_COMPILER}
-        -D MAKEGLOSSARIES_COMPILER_FLAGS=${MAKEGLOSSARIES_COMPILER_FLAGS}
+        -D MAKEGLOSSARIES_COMPILER_ARGS=${MAKEGLOSSARIES_COMPILER_ARGS}
         -P ${LATEX_USE_LATEX_LOCATION}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${latex_build_command}
@@ -1279,7 +1448,7 @@
         -D LATEX_TARGET=${LATEX_TARGET}
         -D MAKEINDEX_COMPILER=${MAKEINDEX_COMPILER}
         -D XINDY_COMPILER=${XINDY_COMPILER}
-        -D MAKEGLOSSARIES_COMPILER_FLAGS=${MAKEGLOSSARIES_COMPILER_FLAGS}
+        -D MAKEGLOSSARIES_COMPILER_ARGS=${MAKEGLOSSARIES_COMPILER_ARGS}
         -P ${LATEX_USE_LATEX_LOCATION}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${pdflatex_build_command}
@@ -1295,7 +1464,7 @@
         -D LATEX_BUILD_COMMAND=makenomenclature
         -D LATEX_TARGET=${LATEX_TARGET}
         -D MAKEINDEX_COMPILER=${MAKEINDEX_COMPILER}
-        -D MAKENOMENCLATURE_COMPILER_FLAGS=${MAKENOMENCLATURE_COMPILER_FLAGS}
+        -D MAKENOMENCLATURE_COMPILER_ARGS=${MAKENOMENCLATURE_COMPILER_ARGS}
         -P ${LATEX_USE_LATEX_LOCATION}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${latex_build_command}
@@ -1306,7 +1475,7 @@
         -D LATEX_BUILD_COMMAND=makenomenclature
         -D LATEX_TARGET=${LATEX_TARGET}
         -D MAKEINDEX_COMPILER=${MAKEINDEX_COMPILER}
-        -D MAKENOMENCLATURE_COMPILER_FLAGS=${MAKENOMENCLATURE_COMPILER_FLAGS}
+        -D MAKENOMENCLATURE_COMPILER_ARGS=${MAKENOMENCLATURE_COMPILER_ARGS}
         -P ${LATEX_USE_LATEX_LOCATION}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${pdflatex_build_command}
@@ -1317,13 +1486,13 @@
   if(LATEX_BIBFILES)
     if(LATEX_USE_BIBLATEX)
       if(NOT BIBER_COMPILER)
-	message(SEND_ERROR "I need the biber command.")
+        message(SEND_ERROR "I need the biber command.")
       endif()
       set(bib_compiler ${BIBER_COMPILER})
-      set(bib_compiler_flags ${BIBER_COMPILER_FLAGS})
+      set(bib_compiler_flags ${BIBER_COMPILER_ARGS})
     else()
       set(bib_compiler ${BIBTEX_COMPILER})
-      set(bib_compiler_flags ${BIBTEX_COMPILER_FLAGS})
+      set(bib_compiler_flags ${BIBTEX_COMPILER_ARGS})
     endif() 
     if(LATEX_MULTIBIB_NEWCITES)
       foreach (multibib_auxfile ${LATEX_MULTIBIB_NEWCITES})
@@ -1367,12 +1536,12 @@
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${latex_build_command}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
-        ${MAKEINDEX_COMPILER} ${MAKEINDEX_COMPILER_FLAGS} ${idx_name}.idx)
+        ${MAKEINDEX_COMPILER} ${MAKEINDEX_COMPILER_ARGS} ${idx_name}.idx)
       set(make_pdf_command ${make_pdf_command}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
         ${pdflatex_build_command}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
-        ${MAKEINDEX_COMPILER} ${MAKEINDEX_COMPILER_FLAGS} ${idx_name}.idx)
+        ${MAKEINDEX_COMPILER} ${MAKEINDEX_COMPILER_ARGS} ${idx_name}.idx)
       set(auxiliary_clean_files ${auxiliary_clean_files}
         ${output_dir}/${idx_name}.idx
         ${output_dir}/${idx_name}.ilg
@@ -1430,6 +1599,22 @@
       )
   endif()
 
+  # Check LaTeX output for important warnings at end of build
+  set(make_dvi_command ${make_dvi_command}
+    COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
+    ${CMAKE_COMMAND}
+      -D LATEX_BUILD_COMMAND=check_important_warnings
+      -D LATEX_TARGET=${LATEX_TARGET}
+      -P ${LATEX_USE_LATEX_LOCATION}
+    )
+  set(make_pdf_command ${make_pdf_command}
+    COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
+    ${CMAKE_COMMAND}
+      -D LATEX_BUILD_COMMAND=check_important_warnings
+      -D LATEX_TARGET=${LATEX_TARGET}
+      -P ${LATEX_USE_LATEX_LOCATION}
+    )
+
   # Capture the default build.
   string(TOLOWER "${LATEX_DEFAULT_BUILD}" default_build)
 
@@ -1483,7 +1668,7 @@
     if(DVIPS_CONVERTER)
       add_custom_command(OUTPUT ${output_dir}/${LATEX_TARGET}.ps
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
-        ${DVIPS_CONVERTER} ${DVIPS_CONVERTER_FLAGS} -o ${LATEX_TARGET}.ps ${LATEX_TARGET}.dvi
+        ${DVIPS_CONVERTER} ${DVIPS_CONVERTER_ARGS} -o ${LATEX_TARGET}.ps ${LATEX_TARGET}.dvi
         DEPENDS ${output_dir}/${LATEX_TARGET}.dvi)
       add_custom_target(${ps_target}
         DEPENDS ${output_dir}/${LATEX_TARGET}.ps
@@ -1498,7 +1683,7 @@
         # simply force a recompile every time.
         add_custom_target(${safepdf_target}
           ${CMAKE_COMMAND} -E chdir ${output_dir}
-          ${PS2PDF_CONVERTER} ${PS2PDF_CONVERTER_FLAGS} ${LATEX_TARGET}.ps ${LATEX_TARGET}.pdf
+          ${PS2PDF_CONVERTER} ${PS2PDF_CONVERTER_ARGS} ${LATEX_TARGET}.ps ${LATEX_TARGET}.pdf
           DEPENDS ${ps_target}
           )
         if(NOT LATEX_EXCLUDE_FROM_DEFAULTS)
@@ -1513,10 +1698,10 @@
       set(default_build html)
     endif()
 
-    if(LATEX2HTML_CONVERTER AND LATEX_MAIN_INPUT_SUBDIR)
+    if(HTLATEX_COMPILER AND LATEX_MAIN_INPUT_SUBDIR)
       message(STATUS
-	"Disabling HTML build for ${LATEX_TARGET_NAME}.tex because the main file is in subdirectory ${LATEX_MAIN_INPUT_SUBDIR}"
-	)
+        "Disabling HTML build for ${LATEX_TARGET_NAME}.tex because the main file is in subdirectory ${LATEX_MAIN_INPUT_SUBDIR}"
+        )
       # The code below to run HTML assumes that LATEX_TARGET.tex is in the
       # current directory. I have tried to specify that LATEX_TARGET.tex is
       # in a subdirectory. That makes the build targets correct, but the
@@ -1524,17 +1709,20 @@
       # generated where expected. I am getting around the problem by simply
       # disabling HTML in this case. If someone really cares, they can fix
       # this, but make sure it runs on many platforms and build programs.
-    elseif(LATEX2HTML_CONVERTER)
-      if(USING_HTLATEX)
-        # htlatex places the output in a different location
-        set(HTML_OUTPUT "${output_dir}/${LATEX_TARGET}.html")
-      else()
-        set(HTML_OUTPUT "${output_dir}/${LATEX_TARGET}/${LATEX_TARGET}.html")
-      endif()
+    elseif(HTLATEX_COMPILER)
+      # htlatex places the output in a different location
+      set(HTML_OUTPUT "${output_dir}/${LATEX_TARGET}.html")
       add_custom_command(OUTPUT ${HTML_OUTPUT}
         COMMAND ${CMAKE_COMMAND} -E chdir ${output_dir}
-          ${LATEX2HTML_CONVERTER} ${LATEX2HTML_CONVERTER_FLAGS} ${LATEX_MAIN_INPUT}
-        DEPENDS ${output_dir}/${LATEX_TARGET}.tex
+          ${HTLATEX_COMPILER} ${LATEX_MAIN_INPUT}
+          "${HTLATEX_COMPILER_TEX4HT_FLAGS}"
+          "${HTLATEX_COMPILER_TEX4HT_POSTPROCESSOR_FLAGS}"
+          "${HTLATEX_COMPILER_T4HT_POSTPROCESSOR_FLAGS}"
+          ${HTLATEX_COMPILER_ARGS}
+        DEPENDS
+          ${output_dir}/${LATEX_TARGET}.tex
+          ${output_dir}/${LATEX_TARGET}.dvi
+        VERBATIM
         )
       add_custom_target(${html_target}
         DEPENDS ${HTML_OUTPUT} ${dvi_target}
@@ -1616,6 +1804,11 @@
 if(LATEX_BUILD_COMMAND)
   set(command_handled)
 
+  if("${LATEX_BUILD_COMMAND}" STREQUAL execute_latex)
+    latex_execute_latex()
+    set(command_handled TRUE)
+  endif()
+
   if("${LATEX_BUILD_COMMAND}" STREQUAL makeglossaries)
     latex_makeglossaries()
     set(command_handled TRUE)
@@ -1631,6 +1824,11 @@
     set(command_handled TRUE)
   endif()
 
+  if("${LATEX_BUILD_COMMAND}" STREQUAL check_important_warnings)
+    latex_check_important_warnings()
+    set(command_handled TRUE)
+  endif()
+
   if(NOT command_handled)
     message(SEND_ERROR "Unknown command: ${LATEX_BUILD_COMMAND}")
   endif()
