document.addEventListener('DOMContentLoaded', () => {
    // 1. 초기 로딩: URL에 해시가 있으면 그 페이지 로드, 없으면 home
    const page = window.location.hash.replace('#', '') || 'home';
    loadPage(page);
});

// 페이지 로드 함수
async function loadPage(pageName) {
    const container = document.getElementById('app-container');
    
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
            
            // 오른쪽 콘텐츠 영역 스크롤 맨 위로 (중요)
            const contentArea = document.querySelector('.content-area');
            if (contentArea) {
                contentArea.scrollTo(0, 0);
            }
            
            window.location.hash = pageName;
            console.log(`[System] Successfully loaded: ${pageName}`);
            
            // ★ 여기가 문제였던 부분: 함수 호출
            updateSidebarUI(pageName); 

        }, 200);

        // 모바일이라면 사이드바 닫기
        if (window.innerWidth < 900) {
            const sidebar = document.getElementById('sidebar');
            if (sidebar) sidebar.classList.remove('open');
        }

    } catch (error) {
        console.error('Page load failed:', error);
        container.innerHTML = `
            <div style="text-align:center; padding: 50px; color: #666;">
                <h2>⚠️ Content Not Found</h2>
                <p>Please check if the file 'pages/${pageName}.html' exists.</p>
                <p>Check the console for more details.</p>
            </div>
        `;
        container.style.opacity = '1';
    }
}

// ★ 누락되었던 함수 추가 (사이드바 버튼 활성화)
function updateSidebarUI(pageName) {
    const buttons = document.querySelectorAll('.nav-btn');
    
    // 1. 모든 버튼에서 active 제거
    buttons.forEach(btn => btn.classList.remove('active'));

    // 2. 현재 페이지에 해당하는 버튼 찾기
    // onclick="loadPage('home')" 같은 문자열을 포함하는지 검사
    const activeBtn = Array.from(buttons).find(btn => {
        const onclickAttr = btn.getAttribute('onclick');
        return onclickAttr && onclickAttr.includes(`'${pageName}'`);
    });

    // 3. active 클래스 추가
    if (activeBtn) {
        activeBtn.classList.add('active');
    }
}

// 모바일 사이드바 토글
function toggleSidebar() {
    const sidebar = document.getElementById('sidebar');
    if (sidebar) {
        sidebar.classList.toggle('open');
    }
}