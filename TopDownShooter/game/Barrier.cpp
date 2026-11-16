#include "Barrier.h"
#include <algorithm>
#include <cmath>
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "Model.h"

void Barrier::Initialize()
{
	for (Layer& layer : layers) {
		layer.object = std::make_unique<Object3d>();
		layer.object->Initialize(Object3dCommon::GetInstance());
		layer.object->SetModel(kModel);
	}
	breakObject = std::make_unique<Object3d>();
	breakObject->Initialize(Object3dCommon::GetInstance());
	breakObject->SetModel(kModelBreak);
	Reset();
}

void Barrier::Reset()
{
	count = 0;
	breakTimer = 0.0f;
	breakReady = false;
	for (Layer& layer : layers) {
		layer.appear = 0.0f;
		layer.ready = false;
	}
}

void Barrier::Add()
{
	count++;
	// 新しく見える層は、小さいところからふくらませる
	if (count <= kMaxLayers) {
		layers[static_cast<size_t>(count - 1)].appear = 0.0f;
		// 次の Update で大きさを計算するまでは描かない
		// （計算前の行列のまま描くと、モデルが画面いっぱいに平たく出てしまう）
		layers[static_cast<size_t>(count - 1)].ready = false;
	}
}

bool Barrier::Consume()
{
	if (count <= 0) {
		return false;
	}
	count--;
	// 割れる演出は、一番外側の層の大きさから始める
	breakTimer = kBreakTime;
	breakReady = false; // 同じく、次の Update で計算するまでは描かない
	return true;
}

void Barrier::Update(const Vector3& center, float deltaTime)
{
	time += deltaTime;
	position = { center.x, center.y + kHeight, center.z };

	int32_t shown = (std::min)(count, kMaxLayers);
	for (int32_t i = 0; i < kMaxLayers; ++i) {
		Layer& layer = layers[static_cast<size_t>(i)];
		if (i >= shown) {
			continue;
		}
		layer.appear = (std::min)(1.0f, layer.appear + deltaTime * 4.0f);
		// ふくらむとき、少し大きくなりすぎてから戻る（ポヨンとさせる）
		float pop = 1.0f + 0.25f * std::sin(layer.appear * 3.14159f);
		float radius = (kBaseRadius + kLayerStep * static_cast<float>(i)) * layer.appear * pop;
		// 層ごとに逆向き・違う速さで回すと、重なりが分かりやすい
		float direction = (i % 2 == 0) ? 1.0f : -1.0f;
		float speed = 0.8f + 0.3f * static_cast<float>(i);
		layer.object->SetScale({ radius, radius, radius });
		layer.object->SetRotate({ time * 0.5f * direction, time * speed * direction, 0.0f });
		layer.object->SetTranslate(position);
		layer.object->Update();
		layer.ready = true;
	}

	if (breakTimer > 0.0f) {
		breakTimer -= deltaTime;
		float t = 1.0f - (std::max)(0.0f, breakTimer) / kBreakTime; // 0→1
		float radius = (kBaseRadius + kLayerStep * static_cast<float>(shown)) * (1.0f + t * 1.2f);
		breakObject->SetScale({ radius, radius, radius });
		breakObject->SetRotate({ 0.0f, time * 4.0f, 0.0f });
		breakObject->SetTranslate(position);
		breakObject->Update();
		breakReady = true;
	}
}

void Barrier::SetCamera(Camera* camera)
{
	for (Layer& layer : layers) {
		layer.object->SetCamera(camera);
	}
	breakObject->SetCamera(camera);
}

void Barrier::Draw()
{
	// 半透明で重ねる（テクスチャの透明度を使う。明るい床の上でも見えるように加算ではなくふつうの半透明）
	// 奥のものを隠さないように、深度は書かない
	Object3dCommon::GetInstance()->SetPipelineState(kBlendModeNormal, true);
	// ※色はモデルが持っていて、1フレームの中で途中で変えても最後の色になるので、割れる方は別のモデルにしている
	int32_t shown = (std::min)(count, kMaxLayers);
	// 少しだけ濃さを脈打たせる
	float pulse = 0.8f + 0.2f * std::sin(time * 5.0f);
	ModelManager::GetInstance()->FindModel(kModel)->SetColor({ 1.0f, 1.0f, 1.0f, pulse });
	for (int32_t i = 0; i < shown; ++i) {
		if (layers[static_cast<size_t>(i)].ready) {
			layers[static_cast<size_t>(i)].object->Draw();
		}
	}
	if (breakTimer > 0.0f && breakReady) {
		// 割れた球は、ふくらみながらだんだん透明に
		float fade = (std::max)(0.0f, breakTimer) / kBreakTime;
		ModelManager::GetInstance()->FindModel(kModelBreak)->SetColor({ 1.0f, 1.0f, 1.0f, fade });
		breakObject->Draw();
	}
	// 元の描き方に戻す
	Object3dCommon::GetInstance()->SetPipelineState(kBlendModeNormal, false);
}