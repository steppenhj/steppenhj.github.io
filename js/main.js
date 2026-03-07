/* --- main.js (Clean Version) --- */

/* --- 0. Menu Group Toggle (Collapsible Sidebar) --- */
function toggleMenuGroup(btn) {
    const submenu = btn.nextElementSibling;
    btn.classList.toggle('collapsed');
    submenu.classList.toggle('open');
}

document.addEventListener('DOMContentLoaded', () => {
    // 1. 초기 로딩: URL에 해시가 있으면 그 페이지 로드, 없으면 home
    const page = window.location.hash.replace('#', '') || 'home';
    loadPage(page);
});

// 페이지 로드 함수
async function loadPage(pageName) {
    const container = document.getElementById('app-container');
    
    // 페이드 아웃 효과
    container.style.opacity = '0';

    try {
        // HTML 파일 Fetch
        const response = await fetch(`pages/${pageName}.html`);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        
        const html = await response.text();

        // 내용 교체 (약간의 딜레이로 부드럽게)
        setTimeout(() => {
            container.innerHTML = html;
            container.style.opacity = '1';
            
            // 오른쪽 콘텐츠 영역 스크롤 맨 위로 초기화
            const contentArea = document.querySelector('.content-area');
            if (contentArea) {
                contentArea.scrollTo(0, 0);
            }
            
            window.location.hash = pageName;
            
            // 사이드바 UI 업데이트 (현재 페이지 하이라이트)
            updateSidebarUI(pageName); 

        }, 200);

        // 모바일이라면 페이지 이동 후 사이드바 닫기
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
            </div>
        `;
        container.style.opacity = '1';
    }
}

// 사이드바 버튼 활성화 함수
function updateSidebarUI(pageName) {
    const buttons = document.querySelectorAll('.nav-btn');
    
    // 모든 버튼에서 active 제거
    buttons.forEach(btn => btn.classList.remove('active'));

    // 현재 페이지에 해당하는 버튼 찾기
    const activeBtn = Array.from(buttons).find(btn => {
        const onclickAttr = btn.getAttribute('onclick');
        return onclickAttr && onclickAttr.includes(`'${pageName}'`);
    });

    // active 클래스 추가
    if (activeBtn) {
        activeBtn.classList.add('active');
    }
}

// 사이드바 토글 (데스크탑/모바일 통합)
function toggleSidebar() {
    const sidebar = document.getElementById('sidebar');
    const isMobile = window.innerWidth < 900;

    if (isMobile) {
        sidebar.classList.toggle('open');
    } else {
        sidebar.classList.toggle('closed');
    }
}


/* --- 1. Media Modal (이미지/동영상 확대) --- */
let currentModalVideo = null;

function openModal(type, src) {
    let modal = document.getElementById('mediaModal');
    
    // 모달 HTML이 없으면 동적으로 생성
    if (!modal) {
        document.body.insertAdjacentHTML('beforeend', `
            <div id="mediaModal" class="modal-overlay" onclick="closeModal(event)">
                <span class="modal-close" onclick="closeModal(event)">&times;</span>
                <div class="modal-content-container" id="modalContainer"></div>
            </div>
        `);
        modal = document.getElementById('mediaModal');
    }

    const container = document.getElementById('modalContainer');
    container.innerHTML = ''; // 기존 내용 비우기

    if (type === 'video') {
        container.innerHTML = `
            <video controls autoplay class="modal-media" style="width: 100%; max-width: 800px;">
                <source src="${src}" type="video/mp4">
                Your browser does not support the video tag.
            </video>
        `;
        currentModalVideo = container.querySelector('video');
    } else {
        container.innerHTML = `
            <img src="${src}" class="modal-media" alt="Enlarged Image">
        `;
    }

    modal.style.display = 'flex';
}

function closeModal(event) {
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


/* --- 2. [NEW] Note Modal (엔지니어링 노트 텍스트 팝업) --- */
function openNoteModal(templateId) {
    const modal = document.getElementById('textModal');
    const template = document.getElementById(templateId);
    
    // 모달이나 템플릿이 없으면 중단
    if (!modal || !template) return;

    // HTML 파일 내의 <template> 태그 안의 내용을 가져옴
    const content = template.content.querySelector('div');
    
    // 모달에 내용 채우기 (data-속성 활용)
    document.getElementById('modalTitle').innerHTML = content.getAttribute('data-title');
    document.getElementById('modalDate').innerText = content.getAttribute('data-date');
    document.getElementById('modalBody').innerHTML = content.innerHTML;
    
    modal.style.display = 'flex';
    document.body.style.overflow = 'hidden'; // 배경 스크롤 방지
}

function closeNoteModal(event) {
    if (event.target.id === 'textModal' || event.target.classList.contains('modal-close')) {
        document.getElementById('textModal').style.display = 'none';
        document.body.style.overflow = 'auto'; // 스크롤 복구
    }
}