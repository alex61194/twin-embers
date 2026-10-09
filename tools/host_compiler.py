"""Run host compiler arguments through MSYS only when its compiler is selected."""
import os, shlex, subprocess
from pathlib import Path

def compile_host(command, **options):
    if os.name=='nt' and '/msys2/usr/bin/' in command[0].replace('\\','/'):
        command=[str(Path(command[0]).with_name('bash.exe')),'-lc',
                 shlex.join(part.replace('\\','/') for part in command)]
    options.setdefault('check',True)
    subprocess.run(command,**options)
