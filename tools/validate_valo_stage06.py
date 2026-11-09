#!/usr/bin/env python3
"""Opt-in capture of the actual SDE and native clients; requires a Wayland session.
Usage: python3 tools/validate_valo_stage06.py [output-directory]
No desktop generator or offline filtering is involved.
"""
import os,subprocess,tempfile,time,re,pathlib,signal,sys,shutil
root=pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
out=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'docs/graphics-v2/evidence/etapa-06'
out.mkdir(parents=True,exist_ok=True)
(out/'xwayland-display.txt').unlink(missing_ok=True)
host_display=os.environ.get('WAYLAND_DISPLAY','')
if not host_display:
 raise SystemExit('A real host Wayland session is required')
if not host_display.startswith('/'):
 host_display=str(pathlib.Path(os.environ['XDG_RUNTIME_DIR'])/host_display)
runtime=tempfile.mkdtemp(prefix='valo06-runtime-')
env=os.environ.copy();env.update(XDG_RUNTIME_DIR=runtime,WAYLAND_DISPLAY=host_display,QT_QPA_PLATFORM='wayland',QT_QPA_PLATFORMTHEME='generic',QT_NO_XDG_DESKTOP_PORTAL='1',SGE_VALO_STABILITY_PROBE_DIR=str(out),SGE_VALO_CLIENT_MIGRATION_PROBE='1',SGE_VALO_BACKDROP_BENCHMARK='1')
log=open(out/'sde-vulkan.log','w');p=subprocess.Popen(['dbus-run-session','--','build/systalia_glass_engine'],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);clients=[]
try:
 for i in range(100):
  if pathlib.Path(runtime,'wayland-systalia-0').exists():break
  if p.poll() is not None:break
  time.sleep(.1)
 clientenv=env.copy();clientenv['WAYLAND_DISPLAY']='wayland-systalia-0';clientenv['WINIT_UNIX_BACKEND']='wayland';clientenv['GDK_BACKEND']='wayland';clientenv['QT_WAYLAND_DISABLE_WINDOWDECORATION']='1'
 for name,cmd in [('alacritty',['alacritty','-e','sh','-c','printf "\\033[32mVALO NATIVE TERMINAL\\033[0m\\n"; exec sh']),('wayland-pattern',['build/tests/tst_client_surface_visibility','--visibility-client']),('zenity',['zenity','--info','--title=VALO GTK Wayland','--text=Native GTK client with V2 decorations'])]:
  f=open(out/(name+'.log'),'w');clients.append(subprocess.Popen(cmd,env=clientenv,stdout=f,stderr=subprocess.STDOUT))
 time.sleep(2)
 displayfile=out/'xwayland-display.txt'
 display=displayfile.read_text().strip() if displayfile.exists() else ''
 m=re.match(r':\d+',display)
 if m:
  xe=clientenv.copy();xe['DISPLAY']=m.group(0)
 if m and shutil.which('xmessage'):
  f=open(out/'xmessage.log','w');clients.append(subprocess.Popen(['xmessage','-bg','white','-fg','black','-buttons','OK:0','VALO REAL XWAYLAND CLIENT'],env=xe,stdout=f,stderr=subprocess.STDOUT))
 if m and shutil.which('xprop'):
  with open(out/'xprop-root.txt','w') as xf:
   subprocess.run(['xprop','-root'],env=xe,stdout=xf,stderr=subprocess.STDOUT,timeout=3)
 print('runtime' ,runtime,'XWayland',m.group(0) if m else 'not ready',flush=True)
 result=p.wait(timeout=65)
 print('SDE exit',result,flush=True)
 if result:
  raise SystemExit(result)
 (out/'client-exit-state.txt').write_text(str([(c.args,c.poll()) for c in clients]))
finally:
 for c in clients:
  if c.poll() is None:c.terminate()
 if p.poll() is None:os.killpg(p.pid,signal.SIGTERM)
 p.wait()
 for c in clients:
  c.wait()
 shutil.rmtree(runtime)
