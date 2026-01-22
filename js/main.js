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
// 사이드바 토글 (데스크탑/모바일 통합)
function toggleSidebar() {
    const sidebar = document.getElementById('sidebar');
    const isMobile = window.innerWidth < 900;

    if (isMobile) {
        // 모바일: 기본 닫힘 -> open 클래스로 열기
        sidebar.classList.toggle('open');
    } else {
        // 데스크탑: 기본 열림 -> closed 클래스로 닫기 (숨기기)
        sidebar.classList.toggle('closed');
    }
}


/* --- Modal 기능 (이미지/동영상 확대) --- */
let currentModalVideo = null;

function openModal(type, src) {
    // 1. 모달 요소 생성 (없으면 만들기)
    let modal = document.getElementById('mediaModal');
    if (!modal) {
        document.body.insertAdjacentHTML('beforeend', `
            <div id="mediaModal" class="modal-overlay" onclick="closeModal(event)">
                <span class="modal-close" onclick="closeModal(event)">&times;</span>
                <div class="modal-content-container" id="modalContainer">
                    </div>
            </div>
        `);
        modal = document.getElementById('mediaModal');
    }

    const container = document.getElementById('modalContainer');
    container.innerHTML = ''; // 기존 내용 비우기

    // 2. 타입에 따라 콘텐츠 주입
    if (type === 'video') {
        container.innerHTML = `
            <video controls autoplay class="modal-media" style="width: 100%; max-width: 800px;">
                <source src="${src}" type="video/mp4">
                Your browser does not support the video tag.
            </video>
        `;
        // 비디오 요소 저장 (닫을 때 멈추기 위해)
        currentModalVideo = container.querySelector('video');
    } else {
        container.innerHTML = `
            <img src="${src}" class="modal-media" alt="Enlarged Image">
        `;
    }

    // 3. 모달 보이기
    modal.style.display = 'flex';
}

function closeModal(event) {
    // 배경이나 닫기 버튼을 눌렀을 때만 닫힘 (콘텐츠 클릭 시 안 닫힘)
    if (event.target.id === 'mediaModal' || event.target.classList.contains('modal-close')) {
        const modal = document.getElementById('mediaModal');
        if (modal) {
            modal.style.display = 'none';
            // 비디오 멈추기
            if (currentModalVideo) {
                currentModalVideo.pause();
                currentModalVideo.currentTime = 0;
                currentModalVideo = null;
            }
        }
    }
}