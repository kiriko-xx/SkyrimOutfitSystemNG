# Apparel Preview 转接链审计（未完成修复）

## 同次现场证据

2026-11-17 21:13:39 启动的 bug/SkyrimOutfitSystemNG - 副本.log:15–23：SE1.5.97 原 host+0x302/RVA0x364652 为 E8，指向私有FF25跳板，再跳 ApparelPreview.dll+0x2DB50。第54行仍成对门禁关闭幻化；第8行BodyMorph v4已取得。本次不是接口失败。不能据owner归属认定ApparelPreview导致历史RTTI异常。

## 作者公开源码对照

来源 https://github.com/maartenharms/apparel-preview ，本地只读审计 tmp/apparel-preview-audit；固定提交8a9e7d7878422dbb1ff52f71abf223e7ab285e01（1.2.0）。不代表用户DLL相同版本或相同字节。

- src/BipedHooks.cpp:26–39：四参ApplyAddonThunk(armor,race,awm,bool)，记录thread_local的真实awm后调用g_origApplyAddon；:181–197在24232/24736的已定位call用write_call保存原目标。与现场302转接形状吻合，但尚未做DLL/RVA对应验证。
- :43–69：worn-pass是InventoryChanges*, IItemChangeVisitor&；owner用于player识别。公开注释警告SE rbx可能只是0x14，不能照抄rbx=Actor。
- :124–169：原E8改为call到stub，保留rcx/rdx，r8/r9仅附加rbx/rdi诊断，最终调用旧worn pass。当前SOS直接调用15856会绕过这层包装，可能破坏AP的t_pass范围和预览注入。
- :107–121及:200–217：GetWornMaskThunk先调用保存的原GetWornMask，再添加预览mask。当前SOS按同callee扫描会漏掉这种站点级改钩。
- src/FittingRoomSignal.h公开CLRP/APPV为特定FittingRoom合作协议；APPV定向FittingRoom，不能假装相同plugin身份。即使暂停preview，也不撤销已安装hook，不能解决call-chain丢失。

## 修复边界与下一步

不修改ApparelPreview函数/内存、不以卸载它作为最终方案。不把FF25或模块名当ABI批准，不从第三方项目复制骨架。需要保留每个精确call-site的原直接目标，而不是越过跳板调用地址库函数。Dont predicate的r13、Custom的rbx/model和原call的栈/返回契约须与捕获目标分离核实；不能用同callee的+3A8替代。

应取得用户实际ApparelPreview.dll及其日志用于版本、摘要、RVA2DB50反编译比对，并检查Custom/ Shim同批hook。当前没有该DLL，本地只有公开源码，不能声称其已与现场二进制匹配。未产出新的功能修复包；6937aec仍仅取证。
