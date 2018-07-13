/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "trackersadditiondialog.h"

#include <QBuffer>
#include <QMessageBox>
#include <QStringList>

#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/net/downloadhandler.h"
#include "base/net/downloadmanager.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "guiiconprovider.h"
#include "ui_trackersadditiondialog.h"

TrackersAdditionDialog::TrackersAdditionDialog(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
    : QDialog(parent)
    , m_ui(new Ui::TrackersAdditionDialog())
    , m_torrent(torrent)
{
    m_ui->setupUi(this);
    // Icons
    m_ui->uTorrentListButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
}

TrackersAdditionDialog::~TrackersAdditionDialog()
{
    delete m_ui;
}

QStringList TrackersAdditionDialog::newTrackers() const
{
    QStringList cleanTrackers;
    foreach (QString url, m_ui->textEditTrackersList->toPlainText().split('\n')) {
        url = url.trimmed();
        if (!url.isEmpty())
            cleanTrackers << url;
    }
    return cleanTrackers;
}

void TrackersAdditionDialog::on_uTorrentListButton_clicked()
{
    m_ui->uTorrentListButton->setEnabled(false);
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->download({m_ui->lineEditListURL->text()});
    connect(handler, static_cast<void (Net::DownloadHandler::*)(const QString &, const QByteArray &)>(&Net::DownloadHandler::downloadFinished)
            , this, &TrackersAdditionDialog::parseUTorrentList);
    connect(handler, &Net::DownloadHandler::downloadFailed, this, &TrackersAdditionDialog::getTrackerError);
    // Just to show that it takes times
    setCursor(Qt::WaitCursor);
}

void TrackersAdditionDialog::parseUTorrentList(const QString &, const QByteArray &data)
{
    // Load from torrent handle
    QList<BitTorrent::TrackerEntry> existingTrackers = m_torrent->trackers();
    // Load from current user list
    QStringList tmp = m_ui->textEditTrackersList->toPlainText().split('\n');
    foreach (const QString &userURL, tmp) {
        BitTorrent::TrackerEntry userTracker(userURL);
        if (!existingTrackers.contains(userTracker))
            existingTrackers << userTracker;
    }

    // Add new trackers to the list
    if (!m_ui->textEditTrackersList->toPlainText().isEmpty() && !m_ui->textEditTrackersList->toPlainText().endsWith('\n'))
        m_ui->textEditTrackersList->insertPlainText("\n");
    int nb = 0;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QBuffer::ReadOnly);
    while (!buffer.atEnd()) {
        const QString line = buffer.readLine().trimmed();
        if (line.isEmpty()) continue;

        BitTorrent::TrackerEntry newTracker(line);
        if (!existingTrackers.contains(newTracker)) {
            m_ui->textEditTrackersList->insertPlainText(line + '\n');
            ++nb;
        }
    }

    // To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    m_ui->uTorrentListButton->setEnabled(true);
    // Display information message if necessary
    if (nb == 0)
        QMessageBox::information(this, tr("No change"), tr("No additional trackers were found."), QMessageBox::Ok);
}

void TrackersAdditionDialog::getTrackerError(const QString &, const QString &error)
{
    // To restore the cursor ...
    setCursor(Qt::ArrowCursor);
    m_ui->uTorrentListButton->setEnabled(true);
    QMessageBox::warning(this, tr("Download error"), tr("The trackers list could not be downloaded, reason: %1").arg(error), QMessageBox::Ok);
}

QStringList TrackersAdditionDialog::askForTrackers(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
{
    QStringList trackers;
    TrackersAdditionDialog dlg(parent, torrent);
    if (dlg.exec() == QDialog::Accepted)
        return dlg.newTrackers();

    return trackers;
}