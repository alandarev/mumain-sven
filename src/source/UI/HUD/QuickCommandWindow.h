
#if !defined(AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_)
#define AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_

#pragma once

#include "UI/Core/WindowManager.h"
#include "UI/Dialogs/MessageBox.h"
#include "Render/Models/ZzzBMD.h"
#include "Engine/Object/ZzzCharacter.h"

namespace mu::ui::window
{
    class CQuickCommandWindow : public CObject
    {
        enum IMAGE_LIST
        {
            IMAGE_QUICKCOMMAND_BACK = CMessageBoxMng::IMAGE_MSGBOX_BACK,
            // The window menu's slots: it drew the same frame, line and arrows before its port.
            IMAGE_QUICKCOMMAND_FRAME_MIDDLE = BITMAP_WINDOW_MENU_BEGIN + 1,
            IMAGE_QUICKCOMMAND_FRAME_DOWN,
            IMAGE_QUICKCOMMAND_LINE,
            IMAGE_QUICKCOMMAND_ARROWL,
            IMAGE_QUICKCOMMAND_ARROWR,
            IMAGE_QUICKCOMMAND_FRAME_UP = BITMAP_QUICKCOMMAND_BEGIN,
        };

    public:
        CQuickCommandWindow();
        virtual ~CQuickCommandWindow();

        bool Create(CManager* pNewUIMng, int x, int y);
        void Release();

        void SetPos(int x, int y);

        bool UpdateMouseEvent();
        bool UpdateKeyEvent();
        bool Update();
        bool Render();

        float GetLayerDepth();	//. 2.0f
        float GetKeyEventOrder();	// 10.f;

        void OpenningProcess();
        void ClosingProcess();
        void OpenQuickCommand(const wchar_t* strID, int iIndex, int x, int y);
        void CloseQuickCommand();
        void SetID(const wchar_t* strID);
        void SetSelectedCharacterIndex(int iIndex);

        int SelectedCharacterIndex() const
        {
            return m_iSelectedCharacterIndex;
        }
        int SelectedCommandIndex() const
        {
            return m_iSelectedIndex;
        }
        const wchar_t* TargetName() const
        {
            return m_strID;
        }
        POINT Position() const
        {
            return m_Pos;
        }

    private:
        void LoadImages();
        void UnloadImages();

        void RenderFrame();
        void RenderContents();
        void RenderArrow();

    private:
        CManager* m_pNewUIMng;
        POINT			m_Pos;

        int m_iSelectedIndex;
        wchar_t m_strID[32];
        int m_iSelectedCharacterIndex;
    };
}

#endif // !defined(AFX_NEWUIQUICKCOMMANDWINDOW_H__3A1D6614_8C41_4066_A831_2954B3C461D5__INCLUDED_)
