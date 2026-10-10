"""Independent immutable snapshot copies and task-owned generated staging."""
from contextlib import contextmanager
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def copy_asset(source, destination):
    """Prefer APFS clone extents without sharing mutable file identities."""
    source, destination = Path(source), Path(destination)
    if source.is_symlink() or destination.exists() or destination.is_symlink():
        raise ValueError('snapshot copy needs a regular source and fresh destination')
    if sys.platform == 'darwin':
        result = subprocess.run(['/bin/cp', '-c', str(source), str(destination)], capture_output=True)
        if result.returncode == 0:
            shutil.copystat(source, destination)
            return
        # The destination was created exclusively by this copy attempt. Some
        # external source filesystems cannot clone; keep normal portability.
        destination.unlink(missing_ok=True)
    shutil.copy2(source, destination)


@contextmanager
def generated_staging(parent, prefix='generated-'):
    """Remove only newly created successful scratch; retain failed diagnostics."""
    parent = Path(parent)
    if parent.is_symlink():
        raise ValueError('generated staging parent must not be a symlink')
    parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix=prefix, dir=parent))
    try:
        yield root
    except BaseException:
        raise
    else:
        shutil.rmtree(root)
