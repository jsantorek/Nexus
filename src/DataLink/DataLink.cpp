#include "DataLink.h"

#include "Consts.h"
#include "Shared.h"

namespace DataLink
{
	std::mutex								Mutex;
	std::map<std::string, LinkedResource>	Registry;

	void Free()
	{
		DataLink::Mutex.lock();
		{
			while (Registry.size() > 0)
			{
				const auto& it = Registry.begin();

				if (it->second.Pointer)
				{
					UnmapViewOfFile((LPVOID)it->second.Pointer);
					it->second.Pointer = nullptr;
				}

				if (it->second.Handle)
				{
					CloseHandle(it->second.Handle);
					it->second.Handle = nullptr;
				}

				LogDebug(CH_DATALINK, "Freed shared resource: \"%s\"", it->first.c_str());

				Registry.erase(it);
			}
		}
		DataLink::Mutex.unlock();
	}

	void* GetResource(const char* aIdentifier)
	{
		std::string str = aIdentifier;

		void* result = nullptr;

		DataLink::Mutex.lock();
		{
			if (Registry.find(str) != Registry.end())
			{
				result = Registry[str].Pointer;
			}
		}
		DataLink::Mutex.unlock();

		return result;
	}

	void* ShareResource(const char* aIdentifier, size_t aResourceSize)
	{
		return ShareResource(aIdentifier, aResourceSize, "");
	}
	void* ShareResource(const char* aIdentifier, size_t aResourceSize, const char* aUnderlyingName)
	{
		std::string str = aIdentifier;
		std::string strOverride = aUnderlyingName;

		void* result = nullptr;

		DataLink::Mutex.lock();
		{
			/* resource already exists */
			if (Registry.find(str) != Registry.end())
			{
				result = Registry[str].Pointer;
			}
			else
			{
				/* allocate new resource */
				LinkedResource resource{};
				resource.Size = aResourceSize;

				if (strOverride == "")
				{
					strOverride = str.c_str();
					strOverride.append("_");
					strOverride.append(std::to_string(GetCurrentProcessId()));
				}

				resource.UnderlyingName = strOverride;

				resource.Handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, strOverride.c_str());
				if (resource.Handle == 0)
				{
					resource.Handle = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, static_cast<DWORD>(aResourceSize), strOverride.c_str());
				}

				if (resource.Handle)
				{
					resource.Pointer = MapViewOfFile(resource.Handle, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<DWORD>(aResourceSize));

					Registry[str] = resource;
					result = resource.Pointer;
				}
			}
		}
		DataLink::Mutex.unlock();

		if (strOverride != "")
		{
			LogDebug(CH_DATALINK, "Created shared resource: \"%s\" (with underlying name \"%s\")", str.c_str(), strOverride.c_str());
		}
		else
		{
			LogDebug(CH_DATALINK, "Created shared resource: \"%s\"", str.c_str());
		}

		return result;
	}
}