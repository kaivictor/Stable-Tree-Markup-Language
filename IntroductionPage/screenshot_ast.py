from playwright.sync_api import sync_playwright

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page(viewport={"width": 1400, "height": 900})
    page.goto("http://localhost:5173")
    page.wait_for_load_state("networkidle")
    page.wait_for_timeout(2000)

    # 截取 JSON 对比区 - 点击错误按钮
    json_sec = page.locator("#json")
    if json_sec.count() > 0:
        json_sec.scroll_into_view_if_needed()
        page.wait_for_timeout(500)
        
        # 点击第二个错误按钮（逗号缺失）
        err_btns = json_sec.locator(".err-btn")
        if err_btns.count() > 1:
            err_btns.nth(1).click()
            page.wait_for_timeout(300)
        
        json_sec.screenshot(path="screenshot_json_fixed.png")
        print("JSON 对比截图已保存")

    # 截取 YAML 对比区 - 点击错误按钮
    yaml_sec = page.locator("#yaml")
    if yaml_sec.count() > 0:
        yaml_sec.scroll_into_view_if_needed()
        page.wait_for_timeout(500)
        
        # 点击第一个错误按钮
        err_btns = yaml_sec.locator(".err-btn")
        if err_btns.count() > 0:
            err_btns.first.click()
            page.wait_for_timeout(300)
        
        yaml_sec.screenshot(path="screenshot_yaml_fixed.png")
        print("YAML 对比截图已保存")

    browser.close()
