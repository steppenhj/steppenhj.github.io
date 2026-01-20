document.addEventListener('DOMContentLoaded', () => {
    // 1. 초기 로딩: URL에 해시가 있으면 그 페이지 로드, 없으면 home
    const page = window.location.hash.replace('#', '') || 'home';
    loadPage(page);
});

// 페이지 로드 함수
async function loadPage(pageName) {
    const container = document.getElementById('app-container');
    const buttons = document.querySelectorAll('.nav-btn');

    // 1. 페이드 아웃 효과
    container.style.opacity = '0';

    try {
        // 2. HTML 파일 Fetch
        const response = await fetch(`pages/${pageName}.html`);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        
        const html = await response.text();

        // 3. 내용 교체 (약간의 딜레이로 부드럽게)
        setTimeout(() => {
            container.innerHTML = html;
            container.style.opacity = '1';
            
            // [수정 포인트] 전체 window가 아니라, '오른쪽 콘텐츠 영역'을 스크롤해야 함
            const contentArea = document.querySelector('.content-area');
            if (contentArea) {
                contentArea.scrollTo(0, 0);
            }
            
            window.location.hash = pageName;
            console.log(`[System] Successfully loaded: ${pageName}`);
            updateSidebarUI(pageName);

        }, 200);

        // 5. 사이드바 버튼 활성화 상태 변경
        buttons.forEach(btn => btn.classList.remove('active'));
        // onclick 속성에서 pageName을 찾아서 활성화 (간단 구현)
        const activeBtn = Array.from(buttons).find(b => b.getAttribute('onclick').includes(pageName));
        if (activeBtn) activeBtn.classList.add('active');

        // 모바일이라면 사이드바 닫기
        if (window.innerWidth < 900) {
            document.getElementById('sidebar').classList.remove('open');
        }

    } catch (error) {
        console.error('Page load failed:', error);
        container.innerHTML = `
            <div style="text-align:center; padding: 50px; color: #666;">
                <h2>⚠️ Content Not Found</h2>
                <p>Please check if the file 'pages/${pageName}.html' exists.</p>
                <p>Note: Fetch API does not work with file:// protocol. Use a local server.</p>
            </div>
        `;
        container.style.opacity = '1';
    }
}

// 모바일 사이드바 토글
function toggleSidebar() {
    document.getElementById('sidebar').classList.toggle('open');
}