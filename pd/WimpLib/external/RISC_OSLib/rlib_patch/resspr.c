--- _src	2009-05-31 18:58:59 +0100
+++ _dst	2013-09-15 12:37:25 +0100
@@ -36,6 +36,8 @@
  */
 
 
+#include "include.h" /* for SKS_ACW */
+
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
@@ -64,7 +66,11 @@
 
 static int sprite_load(char *name)
 {
+#ifndef SKS_ACW
     char file_name[40]; /* long enough for <ProgramName$Dir>.SpritesXX */
+#else /* SKS_ACW */
+    char file_name[256]; /* long enough for <ProgramName$Dir>.SpritesXX */
+#endif /* SKS_ACW */
     res_findname(name, file_name);
     
 #ifdef SPRITES11
