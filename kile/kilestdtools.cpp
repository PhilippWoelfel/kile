/***************************************************************************
                          kilestdtools.cpp  -  description
                             -------------------
    begin                : Thu Nov 27 2003
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

#include <qfileinfo.h>
#include <qregexp.h>

#include <kconfig.h>
#include <klocale.h>
#include <kate/document.h>
#include <kstandarddirs.h>

#include "kiletool.h"
#include "kiletoolmanager.h"
#include "kiletool_enums.h"
#include "kilestdtools.h"
#include "kileinfo.h"
#include "kilelistselector.h"
#include "kiledocmanager.h"
#include "kiledocumentinfo.h"
#include "latexoutputinfo.h"

namespace KileTool
{
	Base* Factory::create(const QString & tool, bool prepare /* = true */)
	{
		//perhaps we can find the tool in the config file
		if (m_config->hasGroup(groupFor(tool, m_config)))
		{
			m_config->setGroup(groupFor(tool, m_config));
			QString toolClass = m_config->readEntry("class", "");

			if ( toolClass == "LaTeX")
				return new LaTeX(tool, m_manager, prepare);

			if ( toolClass == "ForwardDVI" )
				return new ForwardDVI(tool, m_manager, prepare);

			if ( toolClass == "ViewHTML" )
				return new ViewHTML(tool, m_manager, prepare);

			if ( toolClass == "ViewBib" )
				return new ViewBib(tool, m_manager, prepare);

			if ( toolClass == "Base" )
				return new Base(tool, m_manager, prepare);

			if ( toolClass == "Compile" )
				return new Compile(tool, m_manager, prepare);

			if ( toolClass == "Convert" )
				return new Convert(tool, m_manager, prepare);

			if ( toolClass == "View" )
				return new View(tool, m_manager, prepare);

			if ( toolClass == "Sequence" )
				return new Sequence(tool, m_manager, prepare);
		}

		//unknown tool, return 0
		return 0L;
	}

	void Factory::writeStdConfig()
	{
		QString from_cfg = KGlobal::dirs()->findResource("appdata", "kilestdtools.rc");
		QString to_cfg = KGlobal::dirs()->saveLocation("config") + "/kilerc";
		KConfig *pCfg = new KConfig(from_cfg, false, false);
		pCfg->copyTo(to_cfg, m_config);
	}

	int LaTeX::m_reRun = 0;

	bool LaTeX::updateBibs()
	{
		KileDocument::Info *docinfo = manager()->info()->docManager()->infoFor(source());
                if ( docinfo )
                {
			if ( manager()->info()->allBibliographies()->count() > 0 )
				return needsUpdate ( baseDir() + "/" + S() + ".bbl" , manager()->info()->lastModifiedFile(docinfo) );
		}

		return false;
	}

	bool LaTeX::updateIndex()
	{
		KileDocument::Info *docinfo = manager()->info()->docManager()->infoFor(source());
		if ( docinfo )
		{
			const QStringList *pckgs = docinfo->packages();
			for ( uint i = 0; i < pckgs->count(); ++i)
				if ( (*pckgs->at(i)) == "makeidx" )
					return needsUpdate ( baseDir() + "/" + S() + ".ind", manager()->info()->lastModifiedFile(docinfo) );
		}

		return false;
	}

	bool LaTeX::finish(int r)
	{
		kdDebug() << "==bool LaTeX::finish(" << r << ")=====" << endl;

		manager()->info()->outputFilter()->setSource(source());
		QString log = baseDir() + "/" + S() + ".log";

		//manager()->info()->outputFilter()->Run( "/home/wijnhout/test.log" );
		int nErrors = 0, nWarnings = 0, nBadBoxes = 0;
		if ( manager()->info()->outputFilter()->Run(log) )
		{
			manager()->info()->outputFilter()->getErrorCount(&nErrors, &nWarnings, &nBadBoxes);
			QString es = i18n("%n error", "%n errors", nErrors);
			QString ws = i18n("%n warning", "%n warnings", nWarnings);
			QString bs = i18n("%n badbox", "%n badboxes", nBadBoxes);

			sendMessage(Info, es +", " + ws + ", " + bs);

			//jump to first error
			if ( nErrors > 0 && (readEntry("jumpToFirstError") == "yes") )
			{
				connect(this, SIGNAL(jumpToFirstError()), manager(), SIGNAL(jumpToFirstError()));
				emit(jumpToFirstError());
			}
		}

		if ( readEntry("autoRun") == "yes" )
		{
			//check for "rerun LaTeX" warnings
			bool reRan = false;
			if ( (m_reRun < 1) && (nErrors == 0) && (nWarnings > 0) )
			{
				int sz =  manager()->info()->outputInfo()->size();
				for (int i = 0; i < sz; ++i )
				{
					if ( (*manager()->info()->outputInfo())[i].type() == LatexOutputInfo::itmWarning
					&&  (*manager()->info()->outputInfo())[i].message().contains("Rerun") )
					{
						reRan = true;
						break;
					}
				}
			}

			if ( reRan ) ++m_reRun;
			else m_reRun = 0;

			bool bibs = updateBibs();
			bool index = updateIndex();

			if ( reRan )
			{
				kdDebug() << "\trerunning LaTeX " << m_reRun << 
endl;
				Base *tool = manager()->factory()->create(name());
				tool->setSource(source());
				manager()->runNext(tool);
			}

			if ( bibs || index )
			{
				Base *tool = manager()->factory()->create(name());
				tool->setSource(source());
				manager()->runNext(tool);

				if ( bibs ) 
				{
					kdDebug() << "\tneed to run BibTeX" << endl;
					tool = manager()->factory()->create("BibTeX");
					tool->setSource(source());
					manager()->runNext(tool);
				}

				if ( index ) 
				{
					tool = manager()->factory()->create("MakeIndex");
					tool->setSource(source());
					manager()->runNext(tool);
				}
			}
		}

		return Compile::finish(r);
	}

	bool ForwardDVI::determineTarget()
	{
		if (!View::determineTarget())
			return false;

		int para = manager()->info()->lineNumber();
		Kate::Document *doc = manager()->info()->activeDocument();
		QString filepath;

		if (doc)
			filepath = doc->url().path();
		else
			return false;

		QString texfile = manager()->info()->relativePath(baseDir(),filepath);
		m_urlstr = "file:"+targetDir()+"/"+target()+"#src:"+QString::number(para+1)+texfile;
		addDict("%dir_target", QString::null);
		addDict("%target", m_urlstr);
		kdDebug() << "==KileTool::ForwardDVI::determineTarget()=============\n" << endl;
		kdDebug() << "\tusing  " << m_urlstr << endl;

		return true;
	}

	bool ViewBib::determineSource()
	{
		kdDebug() << "==ViewBib::determineSource()=======" << endl;
		if (!View::determineSource())
			return false;

		QString path = source(true);

		//get the bibliographies for this source
		const QStringList *bibs = manager()->info()->allBibliographies(manager()->info()->docManager()->infoFor(path));
		kdDebug() << "\tfound " << bibs->count() << " bibs" << endl;
		if (bibs->count() > 0)
		{
			QString bib = bibs->front();
			if (bibs->count() > 1)
			{
				//show dialog
				KileListSelector *dlg = new KileListSelector(*bibs, i18n("Select a bibliography"),i18n("Select a bibliography"));
				if (dlg->exec())
				{
					bib = (*bibs)[dlg->currentItem()];
					kdDebug() << "Bibliography selected : " << bib << endl;
				}
				else
				{
					sendMessage(Warning, i18n("No bibliography selected."));
					return false;
				}
				delete dlg;
			}
			
			QFileInfo info(path);
			setSource(info.dirPath()+"/"+bib+".bib");
		}
		else
		{
			sendMessage(Error, i18n("No bibliographies found."));
			return false;
		}

		return true;
	}

	bool ViewHTML::determineTarget()
	{
		if (target().isNull())
		{
			//setRelativeBaseDir(S());
			QString dir = readEntry("relDir");
			QString trg = readEntry("target");

			if ( !dir.isEmpty() )
			{
				translate(dir);
				setRelativeBaseDir(dir);
			}

			if ( !trg.isEmpty() )
			{
				translate(trg);
				setTarget(trg);
			}

			//auto-detect the file to view
			if ( dir.isEmpty() && trg.isEmpty() )
			{
				QFileInfo file1 = QFileInfo(baseDir() + "/" + S() + "/index.html");
				QFileInfo file2 = QFileInfo(baseDir() + "/" + S() + ".html");

				bool read1 = file1.isReadable();
				bool read2 = file2.isReadable();

				if ( !read1 && !read2 )
				{
					sendMessage(Error, i18n("Unable to find %1 or %2; if you are trying to view some other HTML file, go to Settings->Configure Kile->Tools->ViewHTML->Advanced.").arg(file1.absFilePath()).arg(file2.absFilePath()));
					return false;
				}

				//both exist, take most recent
				if ( read1 && read2 )
				{
					read1 = file1.lastModified() > file2.lastModified();
					read2 = !read1;
				}

				if ( read1 )
				{
					dir = S();
					trg = "index.html";

					translate(dir);
					setRelativeBaseDir(dir);
					translate(trg);
					setTarget(trg);
				}
			}
		}

		return View::determineTarget();
	}
}

#include "kilestdtools.moc"

