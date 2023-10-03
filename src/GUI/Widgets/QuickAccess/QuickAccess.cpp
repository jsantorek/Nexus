#include "QuickAccess.h"

namespace GUI
{
	namespace QuickAccess
	{
		float size = 32.0f;

		float Opacity = 0.5f;

		std::mutex													Mutex;
		std::map<std::string, Shortcut>								Registry;
		std::map<std::string, QUICKACCESS_SHORTCUTRENDERCALLBACK>	RegistrySimple;

		std::thread		AnimationThread;
		bool			IsAnimating			= false;
		bool			IsFadingIn			= false;
		bool			IsHovering			= false;

		bool			VerticalLayout		= false;
		EQAPosition		Location			= EQAPosition::Extend;
		ImVec2			Offset				= ImVec2(((size + 1) * Renderer::Scaling) * 9, 0.0f);

		void Fade()
		{
			IsAnimating = true;
			while (IsAnimating)
			{
				if (IsFadingIn)				{ Opacity += 0.05f; }
				else						{ Opacity -= 0.05f; }

				if (Opacity > 1)			{ Opacity = 1.0f; IsAnimating = false; }
				else if (Opacity < 0.5f)	{ Opacity = 0.5f; IsAnimating = false; }

				Sleep(35);
			}
		}

		void Render()
		{
			bool isActive = false;

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Opacity);

			ImVec2 pos = ImVec2(0.0f, 0.0f);

			switch (Location)
			{
				case EQAPosition::Extend:
					pos.x += (size * Renderer::Scaling) * 9;
					break;
				case EQAPosition::Under:
					pos.y += size * Renderer::Scaling;
					break;
				case EQAPosition::Bottom:
					pos.y += Renderer::Height - (size * 2 * Renderer::Scaling);
					break;
			}

			pos.x += Offset.x;
			pos.y += Offset.y;

			ImGui::SetNextWindowPos(pos);
			if (ImGui::Begin("QuickAccessBar", (bool*)0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar))
			{
				bool menuFound = false;

				unsigned c = 0;
				Mutex.lock();
				{
					for (auto& [identifier, shortcut] : Registry)
					{
						if (shortcut.TextureNormal && shortcut.TextureHover)
						{
							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));

							//LogDebug(CH_QUICKACCESS, "size: %f | c: %d | scale: %f", size, c, Renderer::Scaling);

							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f }); // smol checkbox
							if (VerticalLayout)
							{
								ImGui::SetCursorPos(ImVec2(0, ((size * c) + (c ? 1 : 0)) * Renderer::Scaling));
							}
							else
							{
								ImGui::SetCursorPos(ImVec2(((size * c) + (c ? 1 : 0)) * Renderer::Scaling, 0));
							}

							if (ImGui::ImageButton(!shortcut.IsHovering ? shortcut.TextureNormal->Resource : shortcut.TextureHover->Resource, ImVec2(size * Renderer::Scaling, size * Renderer::Scaling)))
							{
								isActive = true;
								if (shortcut.Keybind.length() > 0)
								{
									Keybinds::Invoke(shortcut.Keybind);
								}
							}
							ImGui::PopStyleVar();
							shortcut.IsHovering = ImGui::IsItemHovered();
							if (shortcut.TooltipText.length() > 0)
							{
								ImGui::TooltipGeneric(shortcut.TooltipText.c_str());
							}

							ImGui::PopStyleColor(3);

							c++;

							if (!menuFound)
							{
								if (identifier == QA_MENU)
								{
									menuFound = true; // simple optimization
									if (ImGui::BeginPopupContextItem("ShortcutsCtxMenu"))
									{
										isActive = true;
										if (RegistrySimple.size() == 0)
										{
											ImGui::TextDisabled("No shortcuts added.");
										}
										else
										{
											ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f }); // smol checkbox
											for (auto& [identifier, shortcut] : RegistrySimple)
											{
												if (shortcut) { shortcut(); }
											}
											ImGui::PopStyleVar();
										}
										ImGui::EndPopup();
									}
									ImGui::OpenPopupOnItemClick("ShortcutsCtxMenu", 1);
								}
							}
						}
					}
				}
				Mutex.unlock();

				bool newHoverState = ImGui::IsWindowHovered() || isActive;
				if (newHoverState != IsHovering)
				{
					if (newHoverState)	{ IsFadingIn = true; }
					else				{ IsFadingIn = false; }

					if (!IsAnimating)
					{
						AnimationThread = std::thread(Fade);
						AnimationThread.detach();
					}
				}
				IsHovering = newHoverState;

			}
			ImGui::End();

			ImGui::PopStyleVar();
		}

		void AddShortcut(const char* aIdentifier, const char* aTextureIdentifier, const char* aTextureHoverIdentifier, const char* aKeybindIdentifier, const char* aTooltipText)
		{
			std::string str = aIdentifier;
			std::string strTexId = aTextureIdentifier;
			std::string strTexHoverId = aTextureHoverIdentifier;
			std::string strKbId = aKeybindIdentifier;
			std::string strTT = aTooltipText;

			Mutex.lock();
			{
				if (Registry.find(str) == Registry.end())
				{
					Texture* normal = TextureLoader::Get(strTexId.c_str());
					Texture* hover = TextureLoader::Get(strTexHoverId.c_str());
					Shortcut sh{};
					sh.TextureNormal = normal;
					sh.TextureHover = hover;
					sh.Keybind = aKeybindIdentifier;
					sh.TooltipText = aTooltipText;
					Registry[str] = sh;

					// keep trying to get the texture for a little bit
					if (normal == nullptr || hover == nullptr)
					{
						std::thread([str, strTexId, strTexHoverId]()
						{
							int tries = 0;
							int amt = 0;
							if (Registry[str].TextureNormal != nullptr) { amt++; }
							if (Registry[str].TextureHover != nullptr) { amt++; }

							LogDebug(CH_QUICKACCESS, "Shortcut \"%s\" was promised 2 textures, but received %d.", str.c_str(), amt);
							Sleep(1000); // first retry after 1s

							while (Registry[str].TextureNormal == nullptr || Registry[str].TextureHover == nullptr)
							{
								if (tries > 10)
								{
									LogWarning(CH_QUICKACCESS, "Cancelled getting textures for shortcut \"%s\" after 10 failed attempts.", str.c_str());
									break;
								}

								if (Registry[str].TextureNormal == nullptr) { Registry[str].TextureNormal = TextureLoader::Get(strTexId.c_str()); }
								if (Registry[str].TextureHover == nullptr) { Registry[str].TextureHover = TextureLoader::Get(strTexHoverId.c_str()); }

								tries++;
								Sleep(5000);
							}

							/* if not all tries were used, then the texture was loaded */
							if (tries <= 10)
							{
								LogDebug(CH_QUICKACCESS, "Shortcut \"%s\" received promised textures after %d attempt(s).", str.c_str(), tries);
								return;
							}

							/* fallback icons */
							Registry[str].TextureNormal = TextureLoader::Get(ICON_GENERIC);
							Registry[str].TextureHover = TextureLoader::Get(ICON_GENERIC_HOVER);

							/* absolute sanity check */
							if (Registry[str].TextureNormal == nullptr || Registry[str].TextureHover == nullptr)
							{
								LogWarning(CH_QUICKACCESS, "Neither promised textures nor fallback textures could be loaded, removing shortcut \"%s\".", str.c_str());
								RemoveShortcut(str.c_str());
							}
						}).detach();
					}
				}
			}
			Mutex.unlock();
		}

		void RemoveShortcut(const char* aIdentifier)
		{
			std::string str = aIdentifier;

			Mutex.lock();
			{
				Registry.erase(str);
			}
			Mutex.unlock();
		}

		void AddSimpleShortcut(const char* aIdentifier, QUICKACCESS_SHORTCUTRENDERCALLBACK aShortcutRenderCallback)
		{
			std::string str = aIdentifier;

			Mutex.lock();
			{
				if (RegistrySimple.find(str) == RegistrySimple.end())
				{
					RegistrySimple[str] = aShortcutRenderCallback;
				}
			}
			Mutex.unlock();
		}

		void RemoveSimpleShortcut(const char* aIdentifier)
		{
			std::string str = aIdentifier;

			Mutex.lock();
			{
				RegistrySimple.erase(str);
			}
			Mutex.unlock();
		}

		int Verify(void* aStartAddress, void* aEndAddress)
		{
			int refCounter = 0;

			Mutex.lock();
			{
				std::vector<std::string> remove;

				for (auto& [identifier, shortcutcb] : RegistrySimple)
				{
					if (shortcutcb >= aStartAddress && shortcutcb <= aEndAddress)
					{
						remove.push_back(identifier);
						refCounter++;
					}
				}
				for (std::string identifier : remove)
				{
					RegistrySimple.erase(identifier);
				}
			}
			Mutex.unlock();

			return refCounter;
		}
	}
}