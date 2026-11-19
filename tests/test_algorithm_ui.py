"""Optional Edge/Playwright GUI verification. Uses a simulated serial endpoint, never hardware."""
import base64
import json
import os
import subprocess
import time
import unittest
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / 'tests/hw_reports'


class AlgorithmUI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ARTIFACTS.mkdir(exist_ok=True)
        cls.server = subprocess.Popen([str(ROOT/'gui/web/FIELD.exe'), '--no-browser', '--port=0'],
                                      stdout=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
        cls.origin = cls.server.stdout.readline().decode().strip().split()[-1]
        cls.pw = sync_playwright().start()
        cls.browser = cls.pw.chromium.launch(channel='msedge', headless=True)

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.pw.stop()
        cls.server.terminate()
        cls.server.wait(timeout=5)

    def test_demo_layout_and_controls(self):
        page=self.browser.new_page(viewport={'width':1440,'height':1100},device_scale_factor=1)
        errors=[]
        page.on('pageerror',lambda error:errors.append(str(error)))
        page.goto(self.origin+'/algorithm.html?demo=1')
        page.get_by_text('演示数据',exact=False).wait_for()
        page.wait_for_function("() => document.getElementById('actual0').firstChild.textContent !== '—'")
        self.assertEqual(page.locator('.node').count(),10)
        page.get_by_role('switch',name='启用 S4 背景补偿',exact=True).uncheck()
        page.wait_for_function("() => document.getElementById('notice').textContent.includes('已旁路')")
        self.assertFalse(page.get_by_role('switch',name='启用 S4 背景补偿',exact=True).is_checked())
        page.get_by_role('switch',name='启用 S4 背景补偿',exact=True).check()
        page.wait_for_timeout(500)
        page.locator('#freeze').click()
        frozen=page.locator('#actual0').inner_text()
        page.wait_for_timeout(350)
        self.assertEqual(page.locator('#actual0').inner_text(),frozen)
        page.locator('#freeze').click()
        with page.expect_download() as downloaded:
            page.locator('#export').click()
        self.assertIn('demo',downloaded.value.suggested_filename)
        # Refill the demo window after switch-reset checks for visual inspection.
        page.locator('#demo').click();page.locator('#demo').click();page.wait_for_timeout(200)
        page.screenshot(path=str(ARTIFACTS/'algorithm-desktop.png'),full_page=True)
        page.set_viewport_size({'width':390,'height':844})
        page.wait_for_timeout(150)
        self.assertTrue(page.evaluate('document.documentElement.scrollWidth <= innerWidth'))
        page.screenshot(path=str(ARTIFACTS/'algorithm-mobile.png'),full_page=True)
        self.assertEqual(errors,[])
        page.close()

    def test_device_protocol_and_ram_node_switch(self):
        page=self.browser.new_page(viewport={'width':1280,'height':1000})
        page.on('pageerror',lambda error:print('Browser error:',error))
        pending=[];sent=[];state={'profile':1,'flags':255,'bypass':0,'calibrated':1,'trace':0,'running':1,'v2_only':1}
        seq=0
        def endpoint(route):
            nonlocal seq
            op=route.request.url.split('/api/')[-1]
            result={}
            if op=='session':result={'token':'test','transport':'windows-serial'}
            elif op=='ports':result={'ports':['SIMULATED']}
            elif op=='connect':result={'connection':'fake'}
            elif op=='write':
                command=route.request.post_data_json['command'];sent.append(command)
                if command=='ML VERSION':pending.append('VERSION proto=2 fw=test board=sim sensors=sim')
                elif command=='ML NODES':pending.append('NODES '+' '.join(f'{k}={v}' for k,v in state.items()))
                elif command.startswith('ML TRACE '):
                    state['trace']=int(command[-1]);pending.append('TRACE '+command[-1])
                elif command.startswith('ML NODE '):
                    _,_,node,enabled=command.split();node=int(node);enabled=int(enabled)
                    if enabled:state['bypass'] &= ~(1<<node)
                    else:state['bypass'] |= 1<<node
                    pending.append(f'NODE s={node} enabled={enabled}')
                elif command=='ML STOP':state['running']=0;pending.append('Stopped')
                elif command=='ML START':state['running']=1;pending.append('Sensor started successfully')
                result={'ok':True}
            elif op=='read':
                time.sleep(.03)
                if state['trace'] and state['running']:
                    seq+=1
                    for i in range(10):
                        run=0 if state['bypass']&(1<<i) else 1
                        pending.append(f'N seq={seq} t={seq*100000} p=1 s={i} run={run} valid=1 err=0 x={10+i} y=2 z=3 rx=4 ry=5 rz=6')
                data=('\r\n'.join(pending)+'\r\n').encode() if pending else b'';pending.clear()
                result={'connected':True,'data':base64.b64encode(data).decode()}
            route.fulfill(status=200,content_type='application/json',body=json.dumps(result))
        page.route('**/api/**',endpoint)
        page.goto(self.origin+'/algorithm.html')
        page.wait_for_function("() => document.querySelector('#port').value === 'SIMULATED'")
        page.locator('#connect').click()
        page.wait_for_function("() => document.getElementById('actual0').firstChild.textContent === '14.000'")
        page.wait_for_function("() => !document.getElementById('sync').disabled")
        self.assertTrue(page.locator('#profile').is_disabled())
        self.assertTrue(page.locator('#profile option[value="0"]').evaluate('(e) => e.hidden && e.disabled'))
        page.get_by_role('switch',name='启用 S4 背景补偿',exact=True).uncheck()
        try:
            page.wait_for_function("() => document.getElementById('stageState').textContent === '节点已旁路'",timeout=6000)
        except Exception:
            print('UI diagnostic:',page.locator('#notice').inner_text(),page.locator('#stageState').inner_text(),sent,state)
            raise
        page.locator('#stop').click();page.wait_for_function("() => document.getElementById('notice').textContent.includes('采集已停止')")
        page.locator('#start').click();page.wait_for_function("() => document.getElementById('notice').textContent.includes('标定完成')")
        page.locator('#connect').click();page.wait_for_function("() => document.getElementById('connect').textContent === '连接设备'")
        self.assertIn('ML NODE 4 0',sent);self.assertIn('ML TRACE 0',sent)
        self.assertFalse(any(c.startswith(('ML CHFLAG','ML SAVECFG','ML SET ')) for c in sent))
        page.close()


if __name__=='__main__':unittest.main(verbosity=2)
