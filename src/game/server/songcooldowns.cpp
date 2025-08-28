#include "gamecontext.h"  
#include <base/system.h>  
  
static NETADDR KeyAddress(NETADDR Addr)  
{  
    if(Addr.type == NETTYPE_WEBSOCKET_IPV4)  
    {  
        Addr.type = NETTYPE_IPV4;  
    }  
    else if(Addr.type == NETTYPE_WEBSOCKET_IPV6)  
    {  
        Addr.type = NETTYPE_IPV6;  
    }  
    Addr.port = 0;  
    return Addr;  
}  
  
int CSongCooldown::SecondsLeft() const  
{  
    return m_Expire - time_timestamp();  
}  
  
CSongCooldowns::CSongCooldowns()  
{  
}  
  
bool CSongCooldowns::SetCooldown(const NETADDR *pAddr, int Seconds)  
{  
    const int64_t Expire = time_timestamp() + Seconds;  
    CSongCooldown &Cooldown = m_Cooldowns[KeyAddress(*pAddr)];  
      
    if(!Cooldown.m_Initialized)  
    {  
        Cooldown.m_Initialized = true;  
        Cooldown.m_Expire = Expire;  
        return true;  
    }  
      
    if(Expire > Cooldown.m_Expire)  
    {  
        Cooldown.m_Expire = Expire;  
    }  
    return true;  
}  
  
bool CSongCooldowns::IsCooldown(const NETADDR *pAddr) const  
{  
    const auto It = m_Cooldowns.find(KeyAddress(*pAddr));  
    if(It == m_Cooldowns.end())  
    {  
        return false;  
    }  
      
    return It->second.m_Expire > time_timestamp();  
}  
  
int CSongCooldowns::GetSecondsLeft(const NETADDR *pAddr) const  
{  
    const auto It = m_Cooldowns.find(KeyAddress(*pAddr));  
    if(It == m_Cooldowns.end())  
    {  
        return 0;  
    }  
      
    return It->second.SecondsLeft();  
}  
  
void CSongCooldowns::CleanupExpired()  
{  
    const int64_t Now = time_timestamp();  
    for(auto It = m_Cooldowns.begin(); It != m_Cooldowns.end();)  
    {  
        if(It->second.m_Expire <= Now)  
        {  
            It = m_Cooldowns.erase(It);  
        }  
        else  
        {  
            ++It;  
        }  
    }  
}