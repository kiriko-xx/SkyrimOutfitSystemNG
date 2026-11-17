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

用户已提供 D:/Skyrim-dev/兼容参考/Apparel Preview/SKSE/Plugins/ApparelPreview.dll，SHA256 d450712ff73ac26f52e75769d37c92934501eb5aae07ab724e346a5ee0b009ed，包meta标0.4.3。REA核RVA2DB50恢复RCX/RDX/R8/R9后尾跳保存目标，安装函数2E0C0捕获原E8；2DD60含worn TLS包装和预览注入。证据build/apparel-043-binary-evidence.json已导出并关闭。不要再要求同一文件；公开1.2.0与该二进制版本分开。

## 未发布结构修正

当前Custom relay以CallSiteSnapshot捕获原五字节/直接目的地，写入前再次安全读取比较，不绕到地址库末端。0x60帧分离shadow及保存槽，保存原返回RAX/XMM0，调用CustomFromInventoryModel(inventory,model)后恢复；角色取owner不再用rbx，model仍保留引擎SE rdi/AE r15的候选契约，未经实机验证。两参worn ABI假设不授权未知外部调用，第5栈参、非标准volatile或unwind仍是限制。

生产快照头真实MSVC C++20 /W4 /WX /UNDEBUG测试通过（符号极值/溢出/字节变更/不追FF25），Custom静态保护、角色/形变只读/发色guard通过；Release BUILD_EXIT0。不是引擎ABI证明。Dont/Mask链及准入尚未修完，总门禁仍可能关闭幻化，无新功能修复包。禁止读取AP私有RVA字段来批准链。
