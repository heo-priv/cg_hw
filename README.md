# cg_hw

Computer Graphics 과제 모음 (Konkuk Univ. / 학번 202111387)

이 문서는 각 과제·프로젝트마다 **(1) 무엇을 하는 프로젝트인지 + 주요 기능과 핵심 코드(발표용),
(2) 어려웠던 점, (3) AI(Claude) 도움을 받은 부분**을 기록하는 로그입니다.
작업하면서 그때그때 남겨두고, 제출/발표용으로는 나중에 다듬습니다.

---

## Assignment 2 — Textured Cubes (`ass/HW2_202111387`)

### 개요
- **요구사항**: 서로 다른 텍스처·크기의 큐브 2개 이상 / 키보드+마우스로 씬 회전·이동 / 새 기능 1개
- **구현 내용**:
  - 벽돌 큐브(크기 1.0, `brick.bmp`) + 체커 큐브(크기 0.6, `checker.bmp`) — 크기·텍스처 모두 다름
  - 키보드(화살표=회전, WASD=이동, Q/E=줌, R=리셋) + 마우스(드래그=회전, 휠=줌)
  - 새 기능: 텍스처를 입힌 **사각뿔(pyramid)** — 큐브가 아닌 다른 형태
- **환경**: Visual Studio 2022, Win32(x86), GLFW + GLEW + GLM (단일 프로젝트 `OpenglViewer.sln`)
- **작업 방식**: 코드 편집은 Mac에서 → GitHub(`git push`) → Windows에서 `git pull` 후 VS에서 빌드/실행

### 주요 기능 / 핵심 코드 (발표용)
단일 프로젝트(`HW2_202111387`, `main.cpp`)지만 요구사항별로 코드가 나뉜다. 디스플레이는 OpenGL(GLFW+GLEW) 렌더 파이프라인 사용.

- **텍스처 로딩 — `loadBMP()`**: 24비트 BMP를 직접 파싱(픽셀은 BGR·아래→위 순)해 OpenGL 텍스처로 업로드하고 밉맵 생성.
  - 발표: "이미지 파일을 GPU 텍스처로 올리는 부분. 큐브마다 다른 BMP를 로드."
- **셰이더 (인라인 GLSL)** — 정점은 화면좌표로 변환하고 UV를 넘기고, 프래그먼트는 그 UV의 텍스처 색을 출력:
  ```glsl
  // vertex                              // fragment
  gl_Position = MVP * vec4(pos,1);       color = texture(sampler, UV).rgb;
  UV = vertexUV;
  ```
- **두 큐브(크기·텍스처 다름) + 새 도형(피라미드) — `drawObject()`**: 같은 그리기 함수에 **모델 행렬과 텍스처만 바꿔** 호출.
  ```cpp
  MVP = Projection * lookAt(vec3(0,0,gDist), origin, up) * model;
  // 큐브1: model = translate(-2.4,0,0),             texBrick   (크기 1.0)
  // 큐브2: model = translate(2.2,0.4,0)*scale(0.6), texChecker (크기 0.6)
  // 피라미드: 별도 정점버퍼(18정점) + texChecker            ← 새 기능(큐브 아닌 도형)
  ```
  - 발표: "크기는 `scale`로, 텍스처는 다른 BMP로 → 요구사항(서로 다른 크기·이미지의 큐브 2개) 충족. 피라미드는 새 도형."
- **키보드 + 마우스 인터페이스** — 씬 전체에 적용되는 변환을 입력으로 갱신:
  ```cpp
  scene = translate(gTransX,gTransY) * rotate(gRotX, X축) * rotate(gRotY, Y축);
  // processKeyboard(): 화살표=회전, WASD=이동, Q/E·휠=줌(gDist), R=리셋
  // 마우스 콜백: 드래그=회전, 스크롤=줌
  ```
- **`#define GLM_FORCE_RADIANS`** (glm include 앞): GLM 0.9.5.4가 각도를 도(degree)로 읽는 문제를 막아 `perspective`/`rotate`가 라디안 기준으로 정상 동작 (아래 어려웠던 점 6 참고).

### 어려웠던 점
1. **Visual Studio 입문** — VS를 처음 써봐서, 여러 파일 중 무엇을 열어야 빌드되는지 몰랐음.
   → 솔루션 파일 `OpenglViewer.sln`을 열고, 플랫폼을 `x86`으로 맞춰야 한다는 걸 배움.
2. **빌드 에러 C1083 `stdio.h` 없음** — 설치 때 저장공간 아끼려고 **Windows 11 SDK를 체크 해제**해서
   기본 C 헤더(`stdio.h`)조차 못 찾는 상태였음. → Windows SDK는 C/C++ 빌드에 필수임을 확인, 설치로 해결.
3. **디스크 공간 부족** — SDK 설치 공간이 모자라서 Disk Cleanup(시스템 파일 정리: Windows Update 정리 등),
   `%temp%` 비우기, 휴지통 비우기로 공간 확보.
4. **링커 에러 (`__imp__vsnprintf`, `MSVCRT` 충돌)** — 템플릿에 들어있던 `glfw3.lib`가 VS2012 시절의
   오래된 라이브러리라, 최신 컴파일러에서 inline 처리된 stdio 함수 심볼을 못 찾았음.
   → 링커 입력에 **`legacy_stdio_definitions.lib`** 추가로 해결.
5. **Mac ↔ Windows 동기화** — 처음엔 폴더를 손으로 복사했는데 번거롭고 헷갈림.
   → GitHub repo로 push/pull 하는 워크플로우로 전환(처음 한 번 clone, 이후 pull만).
6. **화면이 비정상적으로 확대 + 회전이 거의 안 됨 (가장 까다로웠던 버그)** —
   카메라 거리를 16, 80까지 늘려도 물체가 화면을 꽉 채웠음. 스크린샷으로 확인한 결과,
   원인은 **번들된 GLM이 0.9.5.4 버전**이라 각도를 라디안이 아니라 **도(degree)로 해석**한 것.
   코드는 `glm::radians(45)`(≈0.785)를 넘겼는데 GLM이 이걸 0.785도(≈60배 망원)로 받아들여 과확대됨.
   같은 이유로 회전도 거의 안 먹혔음. → glm include 앞에 **`#define GLM_FORCE_RADIANS`** 추가로 한 번에 해결.

### AI(Claude) 도움을 받은 부분
- **빌드/환경 트러블슈팅 전반**: VS에서 열 파일, x86 설정, SDK 미설치(C1083) 원인 파악과 설치 안내,
  디스크 공간 확보 방법, 링커 에러(`legacy_stdio_definitions.lib`) 해결.
- **Mac↔Windows git 동기화 워크플로우** 설정(clone/pull, 커밋·푸시).
- **핵심 버그 진단**: "줌·회전이 이상하다"는 증상에서 출발해 스크린샷·증상 분석으로
  GLM 0.9.5.4의 degree/radian 문제를 특정하고 `GLM_FORCE_RADIANS`로 수정.
  (디버깅 편의를 위해 창 제목에 카메라 거리 `dist`를 실시간 표시하는 코드도 추가.)
- 코드 구조(텍스처 큐브 2개 + 사각뿔 + 키보드/마우스 인터페이스) 작성·정리.

> 메모: 위 GLM 버그는 다른 과제(3~6)에서도 같은 GLM을 쓰면 똑같이 발생할 수 있으니 주의.

---

## Assignment 3 — Ray Tracer (`ass/HW3_202111387`)

### 개요
- **요구사항**: 평면 1개 + 구 3개로 된 씬을 **직접 만든 레이 트레이서**로 렌더. OpenGL 하드웨어 래스터화 금지(OpenGL/GLUT는 결과 이미지를 화면에 띄우는 용도로만). **한 솔루션에 3개 프로젝트**(문제별 1개).
- **구현 내용**:
  - **Q1 (Proj1_RayIntersection)**: 픽셀마다 eye ray 생성 → 모든 물체와 교차 → 최근접 hit → 맞으면 흰색/아니면 검정. 클래스: `Ray`, `Camera`, `Surface`, `Plane`, `Sphere`, `Scene`.
  - **Q2 (Proj2_PhongShading)**: Phong 모델(ambient+diffuse+specular), 점광원(−4,4,−3), 백색 ambient, **shadow ray로 그림자**, 감마 보정.
  - **Q3 (Proj3_MoreLights)**: 새 기능 = **광원 추가**(두 번째 점광원). 셰이딩이 광원 리스트를 순회하도록 짜여 있어 확장 용이.
- **환경**: VS2022, Win32(x86), GLFW+GLEW+GLM. 교수님 제공 샘플(`cg_code3`)을 베이스로 사용.

### 공통 코드 구조 (발표용 핵심)
세 프로젝트가 같은 레이트레이서 뼈대를 공유한다. OpenGL은 **결과 이미지를 띄우는 용도로만** 쓰고 교차·셰이딩은 전부 CPU에서 직접 계산.
- **`Ray`** = `origin + t*dir`. **`Camera::getRay(ix,iy)`** 는 픽셀을 통과하는 eye ray 생성:
  ```cpp
  us = l + (r-l)*(ix+0.5)/nx;   vs = b + (t-b)*(iy+0.5)/ny;
  dir = us*u + vs*v - d*w;       // 카메라는 -w 방향을 봄
  ```
- **`Surface`(추상) → `Plane`/`Sphere`**: 각자 `intersect()`로 교차 t 계산 (구=2차방정식, 평면=`(y₀-o.y)/dir.y`).
- **`Scene::hit()`**: 모든 표면 중 **가장 가까운 교차**를 찾음. **`occluded()`**: 그림자 광선이 막히는지 검사.
- **디스플레이**: `render()`가 `OutputImage`(float RGB)를 채우고 메인 루프가 `glDrawPixels`로 출력. **`j=0`이 화면 맨 아래 행**(OpenGL 좌표).

### 프로젝트별 설명 (발표 포인트 + 핵심 코드)

**Proj1 `RayIntersection` (Q1)** — 교차 여부만 흑백으로.
```cpp
Ray ray = camera.getRay(i, j);
bool hit = scene.trace(ray, 0, INF);     // 최근접 교차가 있나?
color = hit ? white : black;             // 맞으면 흰색, 아니면 검정
```
- 발표: "픽셀마다 광선을 쏘고, 가장 가까운 물체에 맞으면 흰색." → 평면(화면 아래 절반)과 구 3개가 실루엣으로 보임.

**Proj2 `PhongShading` (Q2)** — 색·조명·그림자 추가.
```cpp
vec3 color = ka * Ia;                                 // ambient
Ray shadowRay(p + n*eps, l);                          // 표면→광원
if (!scene.occluded(shadowRay, eps, distToLight)) {   // 그림자가 아니면
    color += kd * I * max(0, dot(n,l));               // diffuse
    color += ks * I * pow(max(0, dot(r,v)), specPow); // specular(Phong)
}
```
- 발표: "물체별 재질(ka/kd/ks)과 점광원(−4,4,−3)으로 Phong 셰이딩. **shadow ray**가 막히면 ambient만 남겨 그림자를 만든다." (빨강·초록·파랑 공, 초록만 하이라이트)

**Proj3 `MoreLights` (Q3, 새 기능)** — 광원 추가.
```cpp
for (const Light& light : scene.lights) { /* diffuse+spec+그림자 합산 */ }
scene.add(Light(vec3(-4,4,-3), vec3(0.5)));   // 광원 1
scene.add(Light(vec3( 4,4,-3), vec3(0.5)));   // 광원 2 (추가)
```
- 발표: "셰이딩이 **광원 리스트를 순회**하도록 짜여 있어 광원 추가가 한 줄. 세기를 0.5로 낮춘 이유 = 1.0이면 한 광원만으로 바닥이 흰색까지 **포화**돼 단일 광원 그림자가 안 보이기 때문." → 공마다 그림자 2개·하이라이트 2개.

### 어려웠던 점
1. **베이스 코드 혼동** — 과제2 환경(`cg_code`)을 복사해 쓰려 했으나, 과제3은 **OpenGL 래스터화 금지 + 3프로젝트 구조**라 맞지 않았음. 교수님이 따로 준 멀티 프로젝트 샘플(`cg_code3`)이 진짜 베이스였고, 그걸 `ass/HW3_202111387`로 교체.
2. **솔루션 이름 충돌** — 과제2와 과제3 솔루션 파일이 둘 다 `OpenglViewer.sln`이라, VS가 이전(과제2) 솔루션을 자동으로 열어서 "프로젝트가 HW2만 보인다"고 헷갈림. → 올바른 폴더의 `.sln`을 직접 열어 해결.
3. **세 번째 프로젝트 추가** — 제출 요건(3프로젝트)에 맞춰 `Proj3`를 솔루션에 정식 등록(새 GUID 부여 + `.sln`/`.vcxproj` 편집)해야 했음.
4. **링커 에러 예방** — 과제2와 같은 옛 `glfw3.lib`라 동일한 `__imp__vsnprintf` 에러가 날 것이라, `legacy_stdio_definitions.lib`를 미리 추가.
5. **다중 광원 그림자가 안 보임 (가장 흥미로웠던 부분)** — 광원 2개인데도 공마다 그림자가 하나만 보이고, 한 광원만 가려진 영역이 흰색으로 묻힘. 원인은 **포화(saturation)**: 광원 세기가 각각 1.0이라 한 개만으로도 바닥이 흰색까지 차서, 한 광원만 가려진 곳도 거의 안 어두워졌던 것. **광원 세기를 0.5로 낮추니** 밝기가 3단계(둘 다 닿음/하나만 가려짐/둘 다 가려짐)로 분리되어 그림자 2개가 또렷이 보임.
6. **빌드 찌꺼기 정리** — 샘플에 `Debug/`·`.exe`·`.pdb`가 섞여 있어 `.gitignore`로 제외.

### AI 도움을 받은 부분
- **레이 트레이서 전체 설계·구현**: 요구된 클래스(`Ray/Camera/Surface/Plane/Sphere/Scene`), eye ray 생성 공식, 구·평면 교차 수식, 최근접 hit 로직.
- **Phong 셰이딩(Q2)**: ambient/diffuse/specular 식, 점광원·ambient 세팅, **shadow ray** 그림자, 감마 보정.
- **다중 광원(Q3)**: 두 번째 광원 추가, 그리고 **"그림자가 한 개만 보인다"는 증상의 원인(포화) 진단 + 광원 세기 조정으로 해결.**
- **VS 3프로젝트 솔루션 구성**: Proj3 생성·GUID 부여·솔루션 등록, x86 빌드 설정, 링커 수정.
- **Mac↔Windows git 동기화** 워크플로우(과제2에서 확립한 push/pull 방식 그대로 사용).

---

## Assignment 4 — Improving the Ray Tracer (`ass/HW4_202111387`)

### 개요
- **목표**: HW3 레이트레이서를 **후처리(post-processing)** 로 개선. **한 솔루션에 4개 프로젝트**(문제별 1개)이고, 각 프로젝트가 이전 것 위에 기능을 **누적**한다.
  - Q1 `RayTracer_v1` → Q2 `+Gamma` → Q3 `+Antialiasing` → Q4 `새 기능(비교)`
- **공통 베이스**: HW3의 레이트레이서. 디스플레이는 `glDrawPixels`로 이미지 버퍼만 띄우고, 교차·셰이딩은 전부 CPU에서 직접 계산(OpenGL 래스터화 미사용).

### 공통 코드 구조 (발표용 핵심)
모든 프로젝트가 공유하는 뼈대:
- **`Ray`**: `origin + t*dir`. 광선 하나.
- **`Camera::getRay(ix,iy)`**: 픽셀을 통과하는 eye ray 생성. 핵심 공식
  ```cpp
  us = l + (r-l)*(ix+0.5)/nx;   vs = b + (t-b)*(iy+0.5)/ny;
  dir = us*u + vs*v - d*w;      // 카메라는 -w 방향을 봄
  ```
- **`Surface`(추상) → `Plane`/`Sphere`**: 각자 `intersect()`로 광선-물체 교차 t를 계산. 구는 2차방정식, 평면은 `t=(y₀-oₓ.y)/dirₐ.y`.
- **`Scene::hit()`**: 모든 표면을 돌며 **가장 가까운 교차**를 찾음. `Scene::occluded()`: 그림자 광선이 막히는지 검사.
- **`shade()`**: Phong 조명 = `ka·Ia + Σ(kd·I·max(0,n·l) + ks·I·max(0,r·v)^p)`, 그림자면 ambient만.
- **렌더 흐름**: `render()`가 픽셀마다 `getRay → scene.hit → shade` 결과를 `OutputImage`(float RGB)에 채우고, 메인 루프가 `glDrawPixels`로 출력.

### 프로젝트별 설명 (발표 포인트 + 핵심 코드)

**Proj1 `RayTracer_v1` (Q1)** — HW3 결과 재현 (교차 + Phong + 그림자), **감마 없음(linear)**.
- 핵심: `shade()`의 Phong 식 + shadow ray. 단일 점광원(−4,4,−3), 흰색 ambient.
- 출력 직전: `color = clamp(color, 0, 1);` (감마 미적용 → Q2와 구분)
- 발표 포인트: "이게 기준(v1). 감마/AA는 이 위에 얹는다." 참조 이미지와 픽셀 단위로 일치.

**Proj2 `Gamma` (Q2)** — Q1 + 감마 보정.
- 핵심 한 줄: `color = pow(clamp(color,0,1), vec3(1.0f/2.2f));`
- 역할: 모니터의 비선형(γ≈2.2) 응답을 보정 → **중간톤이 밝아져** 눈에 자연스럽게 보임. 흑(0)·백(1)은 그대로, 중간만 상승.
- 발표 포인트: "왜 ^(1/2.2)인가" — 디스플레이 감마의 역보정.

**Proj3 `Antialiasing` (Q3)** — Q2 + 픽셀당 64샘플 슈퍼샘플링.
- 핵심 코드:
  ```cpp
  for (int s = 0; s < 64; ++s) {                 // 픽셀 영역 안 랜덤 64개
      Ray ray = camera.getRay(i + jitter(rng), j + jitter(rng));
      if (scene.hit(ray, 0, INF, rec)) sum += shade(...);
  }
  vec3 color = sum / 64.0f;                       // 박스필터 평균(가중치 1/N)
  color = pow(clamp(color,0,1), vec3(1/2.2f));    // 평균은 선형공간, 감마는 맨 마지막
  ```
- 추가한 것: 서브픽셀 광선용 `Camera::getRay(float px,float py)` 오버로드.
- 역할: 경계 픽셀이 물체+배경을 섞어 평균 → **계단현상(jaggies) 제거**.
- 발표 포인트: ① 왜 픽셀 안에서 여러 번 쏘나(경계 부분 표본화), ② **평균을 선형공간에서 한 뒤 감마**를 적용하는 순서가 왜 옳은지.

**Proj4 `Comparison` (Q4, 새 기능)** — 분할 화면 샘플 수 비교.
- 핵심 코드:
  ```cpp
  bool aa = (i >= Width/2);                        // 왼쪽=OFF, 오른쪽=ON
  int  n  = aa ? 64 : 1;
  Ray ray = aa ? camera.getRay(i+jitter(rng), j+jitter(rng))
               : camera.getRay(i, j);              // 1샘플은 픽셀 중심(계단)
  ...
  if (i == Width/2) color = vec3(1,1,0);           // 노란 경계선
  ```
- 역할: **왼쪽(1샘플, 계단) vs 오른쪽(64샘플, 매끈)** 을 한 이미지로 직접 비교.
- 발표 포인트: AA의 효과를 한 장으로 보여줌. (PDF 예시 중 "different sampling rates"에 해당)

### 어려웠던 점
1. **Q1=감마없음 / Q2=감마 분리** — HW3에선 감마를 셰이딩에 섞어 넣었는데, HW4는 단계가 나뉘어 있어 Q1은 감마를 **빼야** 참조 이미지와 일치. 후처리 단계를 명확히 분리해야 했음.
2. **참조 이미지 정확 일치** — Q1은 "이미지와 동일해야 함"이라, specular 모델(Phong `r·v`, power 32)·색·하이라이트 크기가 참조와 맞는지 확인 필요. (PDF를 이미지로 렌더해 대조)
3. **4프로젝트를 한 솔루션에** — 프로젝트마다 새 GUID 부여 + `.sln` 편집으로 등록하는 작업을 4번 반복(스크립트로 자동화).
4. **64샘플 렌더 부하** — 픽셀당 64배라 창이 뜨기까지 지연. `Debug`는 특히 느려서 `Release`(x86) 권장.
5. **PDF 읽기 환경** — 로컬에 `poppler` 미설치라 PDF 페이지 렌더가 안 돼, `brew install poppler` 후 텍스트·이미지 추출로 지시사항·참조 이미지 확인.

### AI 도움을 받은 부분
- **후처리 구현 전부**: 감마 보정, 64샘플 박스필터 안티앨리어싱(서브픽셀 `getRay` 오버로드 포함), 분할 화면 비교.
- **참조 이미지 분석**: PDF의 Q1 그림을 이미지로 추출·대조해 "감마 분리" 결정과 specular 모델 일치 확인.
- **올바른 파이프라인 순서**: AA 평균을 **선형공간에서 한 뒤 감마**를 마지막에 적용.
- **4프로젝트 솔루션 자동 구성**: 프로젝트 복제·GUID·`.sln` 등록을 파이썬으로 처리.
- **빌드/동기화**: 링커 설정, Mac↔Windows git push/pull, self-contained 라이브러리 포함.

---

## Assignment 5 — Software Rasterizer (`ass/HW5_202111387`)

### 개요
- **목표**: **직접 만든 소프트웨어 래스터라이저**로 삼각형으로 분할된 구를 그림. OpenGL 하드웨어 래스터화 금지(OpenGL은 결과 픽셀버퍼를 `glDrawPixels`로 띄우는 용도만). **한 솔루션에 2개 프로젝트**.
  - Q1 `Rasterizer` → Q2 `Wireframe`
- **공통 베이스**: 교수님 제공 `sphere_scene.cpp`(구를 삼각형으로 분할하는 인덱스 버퍼 생성. **정점 배열 채우기는 TODO라 직접 구현**). 환경은 HW4 빌드 환경 복사(VS2022, x86, GLFW+GLEW+GLM).

### 공통 코드 구조 (발표용 핵심)
- **메시 생성 `create_scene()`**: width=32·height=16로 구를 분할 → **정점 450개, 삼각형 868개**. 정점은 구면좌표 `(sinθcosφ, cosθ, −sinθsinφ)`로 채우고, 제공된 인덱스 버퍼로 삼각형을 구성.
- **변환 파이프라인** (강의/Shirley):
  ```
  M = M_viewport · M_ortho · M_persp · M_camera · M_model
  screen = (M · v).xyz / (M · v).w      // 마지막에 w로 나눠 원근분할
  ```
  - **모델**: 단위구 → `scale(2)` 후 `translate(0,0,−7)` (반지름 2, 중심 (0,0,−7))
  - **카메라**: eye=0, u·v·w=월드축, −w 응시 → **항등행렬**
  - **투영**: `l,r,b,t,n,f` (n=−0.1, f=−1000, **n·f가 음수인 Shirley 방식**). `makeProjection()`이 `M_ortho·M_persp` 반환.
  - **뷰포트**: NDC [−1,1] → 1024² 화면.
- **디스플레이**: `render()`가 `OutputImage`(float RGB)를 채우고 `glDrawPixels`로 출력.

### 프로젝트별 설명 (발표 포인트 + 핵심 코드)

**Proj1 `Rasterizer` (Q1)** — 변환 + 삼각형 채우기 + 깊이버퍼, 무음영 흰 구.
```cpp
vec4 q = M * vec4(vertex,1); vec3 scr = q.xyz / q.w;          // 정점 → 화면좌표
// 삼각형마다 bounding box 안의 픽셀에 대해 edge-function(무게중심):
float w0=edge(B,C,p), w1=edge(C,A,p), w2=edge(A,B,p);
if (모두 같은 부호) {                                          // 픽셀이 삼각형 내부
    float depth = (w0*A.z + w1*B.z + w2*C.z)/area;            // ndc.z 보간
    if (depth > depthBuffer[idx]) { depthBuffer[idx]=depth; 흰색 찍기; }  // z-buffer
}
```
- 발표 포인트: ① 변환 파이프라인(모델→카메라→투영→뷰포트, w 나눗셈), ② edge-function으로 삼각형 내부 판정, ③ **깊이버퍼로 가림 처리**(이 컨벤션에선 near가 ndc.z 큼). 출력은 무음영이라 매끈한 흰 원.

**Proj2 `Wireframe` (Q2, 새 기능)** — 선 래스터화(와이어프레임).
```cpp
// 같은 파이프라인으로 정점 투영 후, 채우기 대신 삼각형 3모서리를 선으로:
drawLine(A,B); drawLine(B,C); drawLine(C,A);
// drawLine: DDA — 긴 축 기준 1px씩 전진하며 픽셀 찍기
int steps = ceil(max(|dx|,|dy|));
for (i=0..steps) setPixel(round(p0 + (p1-p0)*i/steps));
```
- 발표 포인트: 같은 변환 파이프라인 재사용 + **DDA 선 래스터화**로 삼각형 메시를 드러냄. 뒷면 컬링 안 해 앞·뒤 모서리가 다 보이는 지구본형 그물망. (PDF 예시 "line rasterization"에 해당)

### 어려웠던 점
1. **제공 코드의 TODO 직접 구현** — `sphere_scene.cpp`는 인덱스 버퍼만 만들고 정점 배열은 비어 있어, 구면좌표 공식으로 정점 450개를 채워야 했음.
2. **변환 파이프라인 부호** — 과제가 `n=−0.1, f=−1000`처럼 **z값(음수)** 으로 near/far를 주는 Shirley 방식이라(흔한 OpenGL의 양수 거리와 반대), 투영행렬·깊이 방향을 헷갈리기 쉬움. (Mac에서 실행을 못 해 **행렬 체인을 손으로 검산**해 정점 (1,0,0)→화면 (658,512)임을 확인하고 진행)
3. **MSVC 특이사항** — `M_PI`는 `#define _USE_MATH_DEFINES` 필요, `<Windows.h>`의 `min`/`max` 매크로 충돌을 막으려 `#define NOMINMAX` 추가.
4. **빈 `.vcxproj` 버그** — Q2 클론 스크립트에서 `open(쓰기).write(open(읽기).read())`로 한 줄에 처리했더니 **쓰기 모드가 파일을 먼저 비운 뒤 읽혀** `Wireframe.vcxproj`가 0바이트가 됨 → VS "루트 요소 없음" 에러. 안전한 read→write 순서로 재생성해 해결.

### AI 도움을 받은 부분
- **래스터라이저 전체 구현**: 제공 메시의 정점 배열 완성, 변환 파이프라인(모델/카메라/투영/뷰포트 행렬), edge-function 삼각형 래스터화, 깊이버퍼, DDA 선 그리기.
- **Shirley 투영행렬·부호 검산**: n·f 음수 컨벤션의 투영/깊이 방향을 손으로 검증.
- **빌드 이슈 진단**: MSVC `_USE_MATH_DEFINES`/`NOMINMAX`, 빈 `.vcxproj` 원인 파악·복구.
- **2프로젝트 솔루션 구성** + Mac↔Windows git 동기화.

---

## Assignment 6 — Shading the Rasterized Sphere (`ass/HW6_202111387`)

### 개요
- **목표**: HW5 래스터라이저로 그린 구를 **Blinn-Phong 셰이딩 3종(Flat/Gouraud/Phong)** + **새 기능(Deferred Shading)** 으로 렌더. **한 솔루션에 4개 프로젝트**.
  - Q1 `FlatShading` → Q2 `GouraudShading` → Q3 `PhongShading` → Q4 `DeferredShading`
- **공통 베이스**: HW5 변환·래스터화·깊이버퍼 파이프라인 그대로. 셰이딩만 추가.

### 공통 코드 구조 (발표용 핵심)
- **파이프라인/래스터화/깊이버퍼**: HW5와 동일 (`M = 뷰포트·투영·카메라·모델`, edge-function 무게중심, z-buffer).
- **Blinn-Phong** 조명 모델:
  ```
  L = ka·Ia + kd·I·max(0, n·l) + ks·I·max(0, n·h)^p
  ```
  재질 `ka(0,1,0)·kd(0,0.5,0)·ks(0.8)·p64`, 점광원 (−4,4,−3) 백색 단위세기, ambient `Ia=0.2`, 마지막에 감마 2.2.
- **법선**: 단위구라서 **정점 법선 = 정점좌표**(이미 단위). 모델변환이 균등 스케일+이동이라 방향 불변.
- **세 셰이딩의 차이 = "조명을 어디서 평가하느냐"**: 면(Flat) / 정점(Gouraud) / 픽셀(Phong).

### 프로젝트별 설명 (발표 포인트 + 핵심 코드)

**Proj1 `FlatShading` (Q1)** — 삼각형당 1색.
```cpp
vec3 centroid = (wA+wB+wC)/3;                       // 삼각형 무게중심(월드)
vec3 faceN = cross(wB-wA, wC-wA);                   // 면 법선(외적)
if (dot(faceN, centroid-center) < 0) faceN = -faceN;// 바깥쪽으로 정렬
vec3 col = shade(centroid, faceN);                  // 면당 1회 평가 → 면 전체에 적용
```
- 발표: **per-triangle(면) 법선**으로 셰이딩 1회 → 각진(faceted) 결과.

**Proj2 `GouraudShading` (Q2)** — 정점당 셰이딩 후 색 보간.
```cpp
vColor[i] = shade(world[i], normalize(gVertices[i])); // 정점마다 색 계산
...
vec3 col = a*cA + bb*cB + c*cC;                       // 픽셀에서 정점 색 보간
```
- 발표: **per-vertex 법선**으로 정점에서 계산 → 보간. 매끈하지만 하이라이트가 정점에서만 샘플돼 **흐려짐**.

**Proj3 `PhongShading` (Q3)** — 위치·법선 보간 후 픽셀당 셰이딩.
```cpp
vec3 P = a*pA + bb*pB + c*pC;        // 위치 보간
vec3 N = a*nA + bb*nB + c*nC;        // 법선 보간(픽셀에서 재정규화)
vec3 col = shade(P, N);              // per-pixel 평가
```
- 발표: **per-pixel(per-fragment) 법선**으로 픽셀마다 계산 → **또렷한 하이라이트**. (Flat→Gouraud→Phong 품질 향상)

**Proj4 `DeferredShading` (Q4, 새 기능)** — 2-pass G-buffer 방식.
```cpp
// ① 지오메트리 패스: 셰이딩 안 하고 G-buffer에 표면 정보만 저장
gPosition[idx]=P;  gNormal[idx]=N;  gAlbedo[idx]=kd;  // (+depth)
// ② 라이팅 패스: G-buffer 읽어 픽셀마다 1회, 모든 광원 합산
for (light : gLights) col += albedo*light.color*max(0,n·l) + ks*light.color*max(0,n·h)^p;
```
- **무엇인가**: 조명을 **나중에(deferred)** 하는 2단계 파이프라인. *무엇이 보이는가(geometry)* 와 *어떻게 빛나는가(lighting)* 를 분리.
  - 포워드(Q1~Q3)는 래스터화하며 즉시 셰이딩 → 가려질 픽셀도 셰이딩 후 버림.
  - 디퍼드는 ① 보이는 표면 정보만 G-buffer에 모으고 ② 그걸 읽어 **보이는 픽셀만** 1회 셰이딩.
- **왜 새 기능인가**: Q1~Q3는 같은 포워드 파이프라인의 변형(평가 위치만 다름)이지만, deferred는 **파이프라인 구조 자체가 다른 별개 기법**. (PDF Q4 예시 "Using other shading models (Deferred shading)"에 해당)
- **demonstrate**: 조명이 지오메트리와 분리돼 **광원 추가가 라이팅 패스에만** 듦 → 광원 3개(흰·주황·파랑)로 여러 색 하이라이트를 보여줌. (단일 광원이면 결과는 Phong과 동일 — 기법만 다름)
- **장단점**: (+) 많은 광원에 효율적·오버드로 제거·후처리 용이 / (−) G-buffer 메모리·반투명/MSAA 까다로움.

### 어려웠던 점
1. **세 셰이딩의 "평가 위치" 구분** — Flat(면)/Gouraud(정점)/Phong(픽셀)에서 무게중심좌표로 무엇을(색 vs 위치·법선) 보간하는지 정확히 구현해야 했음.
2. **Flat 법선 정의** — "per-triangle normal"을 면 법선(외적)으로 잡고, 메시 winding이 일정치 않아 **구 중심 기준 바깥쪽으로 정렬**(flip)해야 했음.
3. **참조 이미지 정확 일치** — "be careful with floating-point" 경고대로, 색·하이라이트가 참조와 같아야 해서 재질/조명/감마를 스펙 그대로 적용. (Mac 실행 불가 → 단계별로 빌드 확인)
4. **Deferred 2-pass 설계** — 포워드를 지오메트리 패스(G-buffer 기록)와 라이팅 패스(G-buffer 소비)로 분리.

### AI 도움을 받은 부분
- **Blinn-Phong 셰이딩 3종 구현**: Flat(면 법선·centroid), Gouraud(정점 색 보간), Phong(위치·법선 보간 후 per-pixel).
- **Deferred shading 파이프라인**: G-buffer(위치/법선/albedo) 지오메트리 패스 + 다중 광원 라이팅 패스, 그리고 개념·장단점 설명(발표 대본).
- **참조 이미지 분석**으로 재질/조명/감마 일치 확인.
- **4프로젝트 솔루션 구성**(복제·GUID·sln 등록) + Mac↔Windows git 동기화 + self-contained 라이브러리.
