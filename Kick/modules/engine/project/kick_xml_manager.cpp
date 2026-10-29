/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

namespace mt
{
    KickXML::KickXML( juce::Identifier ID, const juce::String& FileExtension, const juce::String& Version )
        : id( ID ), extn( FileExtension ), version( Version )
    {
        vt.addListener( this );
    }

    KickXML::~KickXML()
    {
        save();
    }
    void KickXML::setAutoSave(bool enabled)
    {
        autoSave = enabled;
        if(id == IDs::PREFERENCES) return;
        timer.stopTimer(); cancelPendingUpdate();
        if(enabled && dirty) timer.startTimer(autoSaveTimeOutInMS);
    }

    void KickXML::setAutoSaveTime(int milliseconds)
    {
        autoSaveTimeOutInMS = juce::jlimit(50, 3600000, milliseconds);
        if(id != IDs::PREFERENCES && autoSave && dirty) timer.startTimer(autoSaveTimeOutInMS);
    }
    void KickXML::valueTreeChanged()
    {
        dirty = true;
        if( vt.getType() == IDs::PREFERENCES )
            timer.startTimer( prefAutoSaveTimeOutInMS );
        else
        {
            if( !autoSave )
                return;

            if( !timer.isTimerRunning() )
                timer.startTimer( autoSaveTimeOutInMS );
        }
    }

    void KickXML::handleAsyncUpdate()
    {
        timer.stopTimer();
        if(id != IDs::PREFERENCES && !autoSave) return;
        save();
    }

    juce::ValueTree KickXML::loadOrCreate( const juce::File& XMLFIle )
    {
        juce::ScopedLock l( lock );

        const auto target = resolveFile(XMLFIle, extn);
        if(target.isDirectory()) return {};
        juce::ValueTree next;
        if(target.existsAsFile() && target.getSize() > 0)
        {
            next = juce::ValueTree::fromXml(target.loadFileAsString());
            if(!next.hasType(id)) return {};
        }
        else
        {
            next = juce::ValueTree{id}; next.setProperty(IDs::manufacturer, __manufacturer, nullptr); next.setProperty(IDs::version, version, nullptr);
            if(target.getParentDirectory().createDirectory().failed()) return {};
            juce::TemporaryFile temporary(target);
            if(!writeDocument(temporary.getFile(), next.toXmlString()) || !temporary.overwriteTargetFileWithTemporary()) return {};
        }
        close();
        xmlFile = target; xmlFileDir = target.getParentDirectory(); vt = next; dirty = false; lastSaveSuccessful = true;

        onLoad(); // Invoke callback.
        return vt;
    }

    void KickXML::save()
    {
        juce::ScopedLock l(lock);
        if(xmlFile == juce::File() || xmlFile.isDirectory() || !vt.hasType(id)) { lastSaveSuccessful = false; return; }
        const auto xml = vt.toXmlString().replace("\r\n", "\n");
        if(xmlFile.existsAsFile() && xmlFile.loadFileAsString() == xml)
        {
            lastSaveSuccessful = true; dirty = false; return;
        }
        juce::TemporaryFile temporary(xmlFile);
        lastSaveSuccessful = writeDocument(temporary.getFile(), xml) && temporary.overwriteTargetFileWithTemporary();
        if(lastSaveSuccessful) dirty = false;
    }

    bool KickXML::writeDocument(const juce::File& file, const juce::String& xml)
    {
        auto stream = file.createOutputStream(); if(!stream) return false;
        const bool written = stream->writeText(xml, false, false, "\n"); stream->flush();
        return written && stream->getStatus().wasOk();
    }

    void KickXML::close()
    {
        timer.stopTimer(); cancelPendingUpdate();
        save();
        juce::ScopedLock l( lock );

        xmlFile    = "";
        xmlFileDir = "";
    }

}
