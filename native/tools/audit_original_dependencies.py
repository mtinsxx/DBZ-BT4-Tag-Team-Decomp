#!/usr/bin/env python3
"""Structural source inventory, NOT a semantic completeness detector.

Records actual definitions, extern declarations, literal assembly markers and
literal empty bodies with source lines. Source remains authoritative: a C body
or a matching build does not establish Android integration/behavior.
"""
from pathlib import Path
import argparse, hashlib, json, re
ROOT=Path(__file__).resolve().parents[2]
GROUPS={
 'battle': ['src/battle/battle.c','src/battle/btl_input.c','src/battle/btl_obj.c','src/battle/bobj_a.c','src/battle/btl_cam.c','src/battle/btl_scene.c','src/battle/btl_seq.c','src/battle/btl_char_mgr.c'],
 'ai':['src/battle/btl_ai_*.c'],
 'fighters':['src/battle/btl_char_*.c'],
 'stage':['src/battle/stg_*.c','src/battle/btl_stage_*.c'],
 'effects':['src/battle/eft_*.c'],
 'menus':['src/menu/*.c'],
 'asset-binding':['src/sys/gfxm_e*.c','src/battle/view_a_d.c'],
}
def inventory():
 result={'schema':1,'scope':'structural inventory only; semantic completeness and native integration are NOT inferred','groups':{}}
 definitions=re.compile(r'^([A-Za-z_][\w\s*]*?\b([A-Za-z_]\w*)\([^;{}]*\))\s*\{',re.M)
 for group,patterns in GROUPS.items():
  modules=[]
  for path in sorted({p for pattern in patterns for p in ROOT.glob(pattern)}):
   source=path.read_text();functions=[]
   for match in definitions.finditer(source):
    name=match[2]
    if name in {'if','while','switch','for'} or match[1].lstrip().startswith('extern '):continue
    tail=source[match.end():];empty=re.match(r'\s*(?:(?://[^\n]*\n|/\*.*?\*/)\s*)*}',tail,re.S) is not None
    functions.append({'name':name,'line':source.count('\n',0,match.start())+1,'literal_empty_body':empty})
   modules.append({'path':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
    'definition_count':len(functions),'literal_empty_bodies':[{'name':f['name'],'line':f['line']} for f in functions if f['literal_empty_body']],'extern_declarations':sorted(set(re.findall(r'^extern\s+[^;\n]*?\b([A-Za-z_]\w*)\([^;\n]*\)',source,re.M))),
    'assembly_markers':re.findall(r'INCLUDE_ASM\(\s*"[^"]+"\s*,\s*([A-Za-z_]\w*)',source),'assembly_markers_with_c_definition':sorted(set(re.findall(r'INCLUDE_ASM\(\s*"[^"]+"\s*,\s*([A-Za-z_]\w*)',source))&{f['name'] for f in functions}),'disabled_regions':len(re.findall(r'^\s*#if\s+0\b',source,re.M))})
  result['groups'][group]=modules
 return result
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--check',action='store_true');args=parser.parse_args()
 text=json.dumps(inventory(),indent=2,ensure_ascii=False)+'\n'
 if args.check:
  if not args.output.exists() or args.output.read_text()!=text:raise SystemExit('Source inventory changed; inspect dependencies and regenerate the structural audit')
 else:args.output.write_text(text)
 print('Original subsystem source inventory verified; no semantic-completeness claim')
