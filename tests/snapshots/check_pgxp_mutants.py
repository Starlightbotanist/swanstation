"""Require actual precision consumers to reject omitted PGXP snapshot restores."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cxx',default='c++')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
suite=Path(__file__).resolve().parent
root=suite.parents[1]
out=args.output.resolve()
out.mkdir(parents=True,exist_ok=False)
original=(root/'src/core/pgxp.cpp').read_text(encoding='utf-8')
test=(suite/'pgxp_test.cpp').read_text(encoding='utf-8')
mutations={
    'memory':('  RestorePages(Mem, PGXP_MEM_SIZE, state->memory,\n               current_snapshot ? &current_snapshot->memory : nullptr, memory_dirty.data());',
              '  std::fputs("MUTANT memory hit\\n",stderr);'),
    'vertices':('  if (vertexCache)\n    RestorePages(vertexCache, VERTEX_CACHE_SIZE, state->vertices,\n                 current_snapshot ? &current_snapshot->vertices : nullptr, vertices_dirty.data());',
                '  if (vertexCache) std::fputs("MUTANT vertices hit\\n",stderr);'),
    'cpu':('  std::memcpy(CPU_reg, state->cpu.data(), sizeof(CPU_reg));','  std::fputs("MUTANT cpu hit\\n",stderr);'),
    'gte':('  std::memcpy(GTE_data_reg, state->gte_data.data(), sizeof(GTE_data_reg));','  std::fputs("MUTANT gte hit\\n",stderr);')}
rows=[]
for name in ['control',*mutations]:
    source=original
    if name!='control':
        before,after=mutations[name]
        assert source.count(before)==1,name
        source=source.replace(before,after,1)
    path=out/(name+'-pgxp.cpp')
    path.write_text(source,encoding='utf-8')
    driver=out/(name+'-test.cpp')
    driver.write_text(test.replace('../../src/core/pgxp.cpp',path.as_posix()),encoding='utf-8')
    exe=out/(name+('.exe' if os.name=='nt' else ''))
    command=[args.cxx,'-std=c++17','-O2','-ffunction-sections','-fdata-sections',
             '-I'+str(root/'src'),'-I'+str(root/'src/core'),'-I'+str(root/'dep/libretro-common/include'),
             str(driver),'-Wl,--gc-sections','-o',str(exe)]
    with (out/(name+'-build.log')).open('wb') as log:
        subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
    result=subprocess.run([str(exe)],capture_output=True)
    text=(result.stdout+result.stderr).decode('utf-8',errors='replace')
    (out/(name+'.log')).write_text(text,encoding='utf-8')
    if name=='control':assert result.returncode==0,text
    else:
        assert result.returncode!=0 and 'MUTANT '+name+' hit' in text,text
        assert '== expected' in text or '== expected_cache' in text,text
    row=dict(name=name,exit_code=result.returncode,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    if name=='vertices':
        negative=subprocess.run([str(exe),'--no-cache'],capture_output=True)
        assert negative.returncode==0 and b'MUTANT' not in negative.stderr
        row['inactive_vertex_cache_negative_passed']=True
    rows.append(row)
    (out/'results.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print(name,'PASS' if name=='control' else 'detected',flush=True)
