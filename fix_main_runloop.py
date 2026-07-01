import re
with open("src/main.c", "r") as f:
    code = f.read()

# Add declaration for macwi_cocoa_run_loop
if "void macwi_cocoa_run_loop(void);" not in code:
    code = code.replace('#include "loader/pe_loader.h"', '#include "loader/pe_loader.h"\n\nvoid macwi_cocoa_run_loop(void);')

def replace_func(m):
    return """printf("[macwi] Emulator thread detached. Main thread running Cocoa runloop...\\n");
        macwi_cocoa_run_loop();
        return 0;"""

code = re.sub(r'printf\("\[macwi\] Emulator thread detached.*?return 0;', replace_func, code, flags=re.DOTALL)
with open("src/main.c", "w") as f:
    f.write(code)
