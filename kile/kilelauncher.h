/***************************************************************************
                          kilelauncher.h  -  description
                             -------------------
    begin                : mon 3-11 20:40:00 CEST 2003
    copyright            : (C) 2003 by Jeroen Wijnhout
    email                : Jeroen.Wijnhout@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KILE_LAUNCHER
#define KILE_LAUNCHER

#include <qobject.h>
#include <qdict.h>
#include <qstringlist.h>

class QWidgetStack;


class KProcess;
class KShellProcess;
class KileInfo;
namespace KParts { class ReadOnlyPart; class PartManager; }

namespace KileTool
{
	class Base;
	
	/**
	 * This class represents a way to launch a tool (this could be a commandline tool
	 * running in a Konsole, running as a separate process, it could even be responsible
	 * for starting a kpart.
	 *
	 * @author Jeroen Wijnhout
	 **/
	class Launcher : public QObject
	{
		Q_OBJECT

	public:
		Launcher();
		~Launcher();

	public slots:
		virtual bool launch() = 0;
		virtual bool kill() = 0;
		virtual bool selfCheck() = 0;

	public:
		virtual void setWorkingDirectory(const QString &) {}

		void setParamDict(QDict<QString> *dict) { m_pdictParams = dict; }
		QDict<QString>* paramDict() { return m_pdictParams; }
		void translate(QString &str);

		void setTool(Base *tool) { m_tool = tool; }
		Base* tool() { return m_tool; }
		
	signals:
		void message(int, const QString & );
		void output(char *, int);

		void exitedWith(int);
		void abnormalExit();

		void done(int);

	private:
		QDict<QString>	*m_pdictParams;
		Base			*m_tool;
	};

	class ProcessLauncher : public Launcher
	{
		Q_OBJECT

	public:
		ProcessLauncher(const char * shellname =0);
		~ProcessLauncher();

	public:
		void setWorkingDirectory(const QString &wd);
		void changeToWorkingDirectory(bool change) { m_changeTo = change; }
		void setCommand(const QString & cmd) { m_cmd = cmd; }
		
	public slots:
		bool launch();
		bool kill();
		bool selfCheck() { return true; }

	private slots:
		void slotProcessOutput(KProcess*, char*, int );
		void slotProcessExited(KProcess*);
		
	private:
		QString			m_wd, m_cmd;
		KShellProcess		*m_proc;
		bool				m_changeTo;
	};

	class PartLauncher : public Launcher
	{
		Q_OBJECT

	public:
		PartLauncher(const char * = 0);
		~PartLauncher();

		void setLibrary(const char *lib) { m_libName = lib; }
		void setClass(const char *clas) { m_className = clas; }
		void setOptions(QString & options) { m_options = options; }

	public slots:
		bool launch();
		bool kill();
		bool selfCheck() { return true; }

		KParts::ReadOnlyPart* part() { return m_part; }

	protected:
		KParts::ReadOnlyPart	*m_part;

		QString				m_state;
		const char			*m_name, *m_libName, *m_className;
		QString				m_options;
	};

	class DocPartLauncher : public PartLauncher
	{
		Q_OBJECT

	public:
		DocPartLauncher(const char * name = 0) : PartLauncher(name) {}
		
	public slots:
		bool launch();
	};
}

#endif
