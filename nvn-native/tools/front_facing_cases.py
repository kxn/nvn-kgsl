"""Authored RAW-NATIVE matrix/oracle, not an NVN or game behavior declaration.
Winding uses actual post-viewport window coordinates, the pinned Qualcomm
GRAS_SU_CNTL.FRONT_CW contract, and IR3 hardware0front/-1back conversion.
No rasterizer/image or shader interpreter is implemented here.
"""
def cases():
 out=[]
 for origin in range(2):
  for front_face in range(2):
   for cull in range(4):
    for winding in range(2):
     for indexed in range(2):out.append(dict(case=len(out),window_origin=origin,front_face=front_face,cull_face=cull,winding=winding,indexed=indexed,primitive=4,count=3,instances=1,program='authored-FACE-only',replays=2))
 return out
def outcome(case,repeat):
 if repeat not in(0,1):raise ValueError('two actual same-job VBO replays only')
 # CCW original fullscreen triangle, opposite VBO order on second replay.
 clockwise=bool(case['winding']^repeat^case['window_origin']);front=clockwise==(case['front_face']==0)
 culled=bool(case['cull_face']&(1 if front else 2))
 return dict(front_facing=front,culled=culled,rgba8=[0,0,0,0]if culled else[255,0,0,255]if front else[0,255,0,255])
if __name__=='__main__':
 import argparse,hashlib,json
 from pathlib import Path
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 if a.out.exists():raise ValueError('fresh output required')
 c=dict(window_origin=0,front_face=1,cull_face=0,winding=0)
 assert outcome(c,0)==dict(front_facing=True,culled=False,rgba8=[255,0,0,255])
 assert outcome(c,1)==dict(front_facing=False,culled=False,rgba8=[0,255,0,255])
 assert outcome(dict(c,window_origin=1),0)['front_facing']is False
 assert outcome(dict(c,front_face=0),0)['front_facing']is False
 assert outcome(dict(c,front_face=0,window_origin=1),0)['front_facing']is True
 assert outcome(dict(c,cull_face=1),0)['culled']is True
 assert outcome(dict(c,cull_face=2),1)['culled']is True
 assert all(outcome(dict(c,cull_face=3),r)['culled']for r in(0,1))
 rows=[dict(c,outcomes=[outcome(c,r)for r in(0,1)])for c in cases()];visible=sum(not q['culled']for c in rows for q in c['outcomes']);assert len(rows)==64 and visible==64
 result=dict(schema=1,kind='authored-native-front-facing-matrix-v1',gpu_execution=False,android_execution=False,public_NVN_qualified=False,game_execution=False,planned_jobs=64,planned_draws=128,same_job_replays=64,expected_visible_draws=64,expected_culled_draws=64,hand_controls=8,cases=rows,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),boundary='Native post-viewport facing only. Public Call/Copy GPU qualification requires actual complete Maxwell resources; do not equate a raw IB with public Call.')
 a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='cases'}))
