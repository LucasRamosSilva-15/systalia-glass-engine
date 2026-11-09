#!/usr/bin/env python3
"""Capture header alignment in the actual Vulkan SDE inside a host Wayland session.
Usage: python3 tools/validate_valo_header.py [after]
"""
import pathlib,os,tempfile,subprocess,time,signal,sys
root=pathlib.Path(__file__).resolve().parents[1];os.chdir(root)
out=root/'docs/graphics-v2/evidence/etapa-06-1'/(sys.argv[1] if len(sys.argv)>1 else 'after');out.mkdir(parents=True,exist_ok=True)
runtime=tempfile.TemporaryDirectory(prefix='valo061-');env=os.environ.copy();host=env['WAYLAND_DISPLAY'];host=host if host.startswith('/') else env['XDG_RUNTIME_DIR']+'/'+host
env.update(XDG_RUNTIME_DIR=runtime.name,WAYLAND_DISPLAY=host,QT_QPA_PLATFORM='wayland',QT_QPA_PLATFORMTHEME='generic',QT_NO_XDG_DESKTOP_PORTAL='1',SGE_VALO_STABILITY_PROBE_DIR=str(out),SGE_VALO_HEADER_ALIGNMENT_PROBE='1')
with open(out/'sde.log','w') as log:
 p=subprocess.Popen(['dbus-run-session','--','build/systalia_glass_engine'],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);c=None
 try:
  for i in range(100):
   if pathlib.Path(runtime.name,'wayland-systalia-0').exists():break
   if p.poll() is not None:raise RuntimeError('SDE startup failed')
   time.sleep(.1)
  ce=env.copy();ce.update(WAYLAND_DISPLAY='wayland-systalia-0',WINIT_UNIX_BACKEND='wayland')
  with open(out/'alacritty.log','w') as cl:
   c=subprocess.Popen(['alacritty','-e','sh','-c','(sleep 6; printf "\\033]0;Alacritty — título longo para verificar truncamento sem sobrepor os controles da janela VALO\\007") & exec sh'],env=ce,stdout=cl,stderr=subprocess.STDOUT)
   result=p.wait(timeout=40);print('SDE',result)
   if result:raise SystemExit(result)
 finally:
  if c is not None and c.poll() is None:c.terminate();c.wait()
  if p.poll() is None:os.killpg(p.pid,signal.SIGTERM);p.wait()
runtime.cleanup()
